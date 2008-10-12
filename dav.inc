<?php
// $Id$

/**
 * @file
 * DAV server implementation and API for Drupal.
 */

//////////////////////////////////////////////////////////////////////////////
// DAV constants

define('STATUS_102', '102 Processing');
define('STATUS_200', '200 OK');
define('STATUS_201', '201 Created');
define('STATUS_204', '204 No Content');
define('STATUS_207', '207 Multi-Status');
define('STATUS_403', '403 Forbidden');
define('STATUS_404', '404 Not Found');
define('STATUS_405', '405 Method Not Allowed');
define('STATUS_409', '409 Conflict');
define('STATUS_412', '412 Precondition Failed');
define('STATUS_415', '415 Unsupported Media Type');
define('STATUS_422', '422 Unprocessable Entity');
define('STATUS_423', '423 Locked');
define('STATUS_424', '424 Failed Dependency');
define('STATUS_507', '507 Insufficient Storage');

define('DAV_UPLOAD_KEY', 'dav_upload');

//////////////////////////////////////////////////////////////////////////////
// DAV implementation

/**
 * DAV server implementation based on HTTP_WebDAV_Server from PEAR.
 *
 * @package dav.module
 * @author Arto Bendiken <http://bendiken.net/>
 */
class drupal_dav_server extends HTTP_WebDAV_Server {
  var $http_auth_realm = 'Drupal DAV';
  var $dav_powered_by = 'Drupal (+http://drupal.org/project/dav)';

  /**
   * Called from dav.module to serve an incoming DAV request.
   */
  static function serve($script_name, $path) {
    $server = new drupal_dav_server();
    $server->http_auth_realm = variable_get('site_name', $server->http_auth_realm);
    $server->_SERVER['SCRIPT_NAME'] = $script_name;
    $server->_SERVER['PATH_INFO'] = $path;

    return $server->serverequest();
  }

  /**
   * Callback providing a last opportunity to override the HTTP response
   * status.
   *
   * We only use it to be able to correctly handle PUT requests, which may
   * need arbitrary forms of post-processing that DAV module may provide.
   *
   * @param $status
   *   Status code and message.
   * @return void
   */
  function http_status($status) {
    switch ($this->_SERVER['REQUEST_METHOD']) {
      // Provide post-processing for PUT requests (i.e., file uploads)
      case 'PUT':
        $status = $this->PUT_done($status);
        break;
    }

    // This will send the HTTP status and set the X-WebDAV-Status header
    parent::http_status($status);
  }

  /**
   * Checks a DAV user's authentication credentials using Drupal's
   * extensible authentication mechanism.
   *
   * For DAV clients which support cookie-based sessions, HTTP
   * authentication will only be needed in the form of an initial handshake.
   * Most clients, however, do not support cookies and will send the
   * credentials with every subsequent DAV request, too.
   *
   * @param $type
   *   Authentication type, such as 'basic' or 'digest'. Not used.
   * @param $username
   *   Transmitted user name.
   * @param $password
   *   Transmitted password, as plaintext.
   * @return
   *   A boolean indicating successful or failed authentication status.
   */
  function check_auth($type = NULL, $username = NULL, $password = NULL) {
    global $user;
    if (empty($user)) {
      _dav_trace('AUTH', array('type' => $type, 'username' => $username, 'password' => $password ? t('(hidden)') : NULL));
    }

    // Check if a user name was supplied in the request
    if (!is_null($username)) {
      if (!($user = _dav_authenticate($username, trim($password)))) {
        return FALSE; // Invalid user name or password
      }
    }

    // Check authorization for anonymous or registered user
    $access = user_access('access DAV resources');
    if (!is_null($username) && !$access) {
      // We will only log failed authentication requests, since for
      // successful authentication we have no means to distinguish the
      // initial handshake from every subsequent request which may include
      // HTTP authentication credentials.
      watchdog('dav', 'Login attempt failed for %user.', array('%user' => theme('placeholder', $username)));
    }
    return $access;
  }

  /**
   * Checks the LOCK status for a DAV resource, returning any current shared
   * or exclusive locks obtained on the resource.
   *
   * @param $path
   *   Resource path to check LOCK status for.
   * @return
   *   An array of lock entries, each entry being an array with the keys
   *   'type' ('shared'/'exclusive'), 'token' and 'timeout'.
   */
  function checklock($path) {
    $path = _dav_explode_path($path);
    if (($resource = _dav_resolve($path)) && ($resource_id = dav_intern_resource($resource))) {
      $result = db_query('SELECT l.* FROM {dav_locks} l WHERE l.resource_id = %d', $resource_id);
      if (($lock = db_fetch_object($result))) {
        return array(
          'type'     => !empty($lock->is_writelock) ? 'write' : 'read',
          'scope'    => !empty($lock->is_exclusive) ? 'exclusive' : 'shared',
          'depth'    => $lock->depth,
          'owner'    => $lock->user_id, // TODO: this is technically incorrect
          'token'    => $lock->token,
          'created'  => $lock->created_at,
          'modified' => $lock->updated_at,
          'expires'  => $lock->expires_at,
        );
      }
    }
  }

  /**
   * Implementation of the HTTP GET method.
   *
   * @see http://www.webdav.org/specs/rfc2518.html#METHOD_GET
   */
  function GET(&$options) {
    _dav_trace('get', $options);

    $path = _dav_explode_path($options['path']);

    if ($resource = _dav_resolve($path)) {
      $props = dav_propfind($resource);

      // If a GET handler is provided, assume a file resource
      if (($options = _dav_dispatch('get', $resource, $options)) !== NULL) {
        return $options !== FALSE;
      }

      // Otherwise, the resource should be a collection
      if ($props['resourcetype'] == 'collection') {
        print theme('dav_page', $path, dav_list($resource, $path));
        return FALSE;
      }
    }

    return FALSE;
  }

  /**
   * Implementation of the HTTP PUT method.
   *
   * @see http://www.webdav.org/specs/rfc2518.html#METHOD_PUT
   */
  function PUT(&$options) {
    _dav_trace('put', $options);

    $path = _dav_explode_path($options['path']);
    $name = array_pop($path);

    if (!($parent = _dav_resolve($path)))
      return STATUS_404; // Not Found

    // Prevent hidden file creation if disabled by administrator
    if (!DAV_DOT_FILES && $name[0] == '.')
      return STATUS_403; // Forbidden

    // Prevent metadata proliferation with Windows / Mac OS X clients
    if ((DAV_WINDOWS_NO_THUMBS_DB && strtolower($name) == 'thumbs.db') ||
        (DAV_MACOSX_NO_DS_STORE && $name == '.DS_Store'))
      return STATUS_403; // Forbidden

    // Prevent resource fork creation with Mac OS X clients
    if (DAV_MACOSX_NO_FORKS && strpos($name, '._') === 0)
      return STATUS_201; // Created (faked)

    // Attempt to lookup an existing resource
    if ($resource = dav_lookup($parent, $name)) {
      // This resource will be overwritten
      $options['new'] = FALSE;
      if (!user_access('update DAV resources'))
        return STATUS_403; // Forbidden
    }
    else {
      // This is a new resource
      $options['new'] = TRUE;
      if (!user_access('create DAV resources'))
        return STATUS_403; // Forbidden
    }

    if ($result = _dav_dispatch('put', $parent, $name, $options, NULL)) {
      if ($result !== TRUE)
        return $result; // Failed with status code
      $filename = _dav_tmpname();
      $this->_put_args = array($parent, $name, $options, $filename);
      return fopen($filename, 'wb'); // Success
    }

    return STATUS_409; // Conflict
  }

  function PUT_done($status) {
    if (empty($this->_put_args))
      return $status;

    $args = array_merge(array('put'), $this->_put_args);
    if ($result = call_user_func_array('_dav_dispatch', $args)) {
      $filename = array_pop($args);
      if (file_exists($filename))
        @unlink($filename); // clean up if needed
      return is_string($result) ? $result : $status;
    }

    return STATUS_409; // Conflict
  }

  /**
   * Implementation of the DAV COPY method.
   *
   * @see http://www.webdav.org/specs/rfc2518.html#METHOD_COPY
   */
  function COPY($options) {
    _dav_trace('copy', $options);

    $source_path = _dav_explode_path($options['path']);
    $target_path = _dav_explode_path($options['dest']);
    $depth = !empty($options['depth']) ? $options['depth'] : 0;

    // Ensure the source collection or resource actually exists
    if (!($source = _dav_resolve($source_path)))
      return STATUS_404; // Not Found

    // Perform sanity checks and overwrite target if needed
    if ($error = $this->_prepare_copy_or_move($options, $target_path))
      return $error;

    if (!user_access('create DAV resources'))
      return STATUS_403; // Forbidden

    if (!_dav_dispatch('copy', $source, $source_path, $target_path))
      return STATUS_409; // Conflict

    return STATUS_201; // Created
  }

  /**
   * Implementation of the DAV MOVE method.
   *
   * @see http://www.webdav.org/specs/rfc2518.html#METHOD_MOVE
   */
  function MOVE($options) {
    _dav_trace('move', $options);

    $source_path = _dav_explode_path($options['path']);
    $target_path = _dav_explode_path($options['dest']);
    $depth = !empty($options['depth']) ? $options['depth'] : 0;

    // Ensure the source collection or resource actually exists
    if (!($source = _dav_resolve($source_path)))
      return STATUS_404; // Not Found

    // Perform sanity checks and overwrite target if needed
    if ($error = $this->_prepare_copy_or_move($options, $target_path))
      return $error;

    // From here on out, we can assume the target doesn't exist
    $source_name = array_pop($source_path);
    $target_name = array_pop($target_path);

    // If the source and target collections are the same, then this becomes a simple rename operation
    if ($source_path == $target_path) {
      if (!user_access('rename DAV resources'))
        return STATUS_403; // Forbidden

      if (($result = _dav_dispatch('rename', $source, $source_name, $target_name)) !== TRUE)
        return is_string($result) ? $result : STATUS_409; // Conflict
    }
    else {
      if (!user_access('move DAV resources'))
        return STATUS_403; // Forbidden

      array_push($source_path, $source_name);
      array_push($target_path, $target_name);
      if (($result = _dav_dispatch('move', $source, $source_path, $target_path)) !== TRUE)
        return is_string($result) ? $result : STATUS_409; // Conflict
    }

    return STATUS_201; // Created
  }

  function _prepare_copy_or_move($options, $target_path) {
    $overwrite = !empty($options['overwrite']);

    // No body parsing implemented yet
    if (!empty($this->_SERVER['CONTENT_LENGTH']))
      return STATUS_415; // Unsupported Media Type

    // No copying to different DAV servers supported
    if (isset($options['dest_url']))
      return STATUS_502; // Bad Gateway

    // Check if the target already exists, and ensure it can be overwritten
    if (($target = _dav_resolve($target_path)) && !$overwrite)
      return STATUS_412; // Precondition Failed

    // Obliterate the target, if necessary and access rights to do so
    if ($target) {
      if (!user_access('delete DAV resources'))
        return STATUS_403; // Forbidden
      if (!_dav_dispatch('delete', $target))
        return STATUS_409;
    }
  }

  /**
   * Implementation of the HTTP DELETE method.
   *
   * @see http://www.webdav.org/specs/rfc2518.html#METHOD_DELETE
   */
  function DELETE($options) {
    _dav_trace('delete', $options);

    if (!user_access('delete DAV resources'))
      return STATUS_403; // Forbidden

    $path = _dav_explode_path($options['path']);
    $name = array_pop($path);

    if ($parent = _dav_resolve($path)) {
      if ($resource = dav_lookup($parent, $name)) {

        if (!_dav_dispatch('delete', $resource, $parent))
          return STATUS_409; // Conflict

        return STATUS_204; // No Content
      }
    }

    return STATUS_404; // Not Found
  }

  /**
   * Implementation of the DAV MKCOL method.
   *
   * @see http://www.webdav.org/specs/rfc2518.html#METHOD_MKCOL
   */
  function MKCOL($options) {
    _dav_trace('mkcol', $options);

    if (!user_access('create DAV resources'))
      return STATUS_403; // Forbidden

    $path = _dav_explode_path($options['path']);
    $name = array_pop($path);

    // Prevent hidden collection creation if disabled by administrator
    if (!DAV_DOT_FILES && $name[0] == '.')
      return STATUS_403; // Forbidden

    if ($parent = _dav_resolve($path)) {

      if ($resource = dav_lookup($parent, $name))
        return STATUS_409; // Conflict

      if ($result = _dav_dispatch('mkcol', $parent, $name))
        return STATUS_201; // Success
    }

    return STATUS_405; // Method Not Allowed
  }

  /**
   * Implementation of the DAV PROPFIND method.
   *
   * @see http://www.webdav.org/specs/rfc2518.html#METHOD_PROPFIND
   */
  function PROPFIND(&$options, &$files) {
    _dav_trace('propfind', $options);

    $path = _dav_explode_path($options['path']);
    if ($parent = _dav_resolve($path)) {
      $depth = !empty($options['depth']) ? $options['depth'] : 0;
      $files['files'] = _dav_resources($path, $parent, $depth);
      return TRUE; // found
    }

    return FALSE; // not found
  }

  /**
   * Implementation of the DAV PROPPATCH method.
   *
   * @see http://www.webdav.org/specs/rfc2518.html#METHOD_PROPPATCH
   */
  function PROPPATCH(&$options) {
    _dav_trace('proppatch', $options);

    $path = _dav_explode_path($options['path']);
    if (!($resource = _dav_resolve($path)))
      return STATUS_404; // Not Found

    if (is_array($options['props'])) {
      foreach ($options['props'] as $key => $prop) {
        if (strtolower($prop['ns']) == 'dav:') {
          $options['props'][$key]['status'] = STATUS_403;
        }
        else if (isset($prop['val'])) {
          dav_update_property($resource, $prop['ns'], $prop['name'], $prop['val']);
        }
        else {
          dav_delete_property($resource, $prop['ns'], $prop['name']);
        }
      }
    }

    return ''; // TODO: check return value handling in PEAR base class
  }

  /**
   * Implementation of the DAV LOCK method.
   *
   * @see http://www.webdav.org/specs/rfc2518.html#METHOD_LOCK
   */
  function LOCK(&$options) {
    _dav_trace('lock', $options);

    $path = _dav_explode_path($options['path']);
    $name = end($path);

    if (!($resource = _dav_resolve($path)))
      return STATUS_404; // Not Found

    // Recursive locks on collections not supported yet
    if (!empty($options['depth']))
      return STATUS_409; // Conflict

    // Prevent hidden file creation if disabled by administrator
    if (!DAV_DOT_FILES && $name[0] == '.')
      return STATUS_403; // Forbidden

    // Prevent metadata proliferation with Windows / Mac OS X clients
    if ((DAV_WINDOWS_NO_THUMBS_DB && strtolower($name) == 'thumbs.db') ||
        (DAV_MACOSX_NO_DS_STORE && $name == '.DS_Store'))
      return STATUS_403; // Forbidden

    // Prevent resource fork creation with Mac OS X clients
    if (DAV_MACOSX_NO_FORKS && strpos($name, '._') === 0)
      return STATUS_200; // (faked)

    $options['timeout'] = time() + 600; // 10 minutes (hardcoded)

    if (!empty($options['update'])) {
      if (dav_renew_lock($resource, $options['locktoken'], $options['timeout'])) {
        return STATUS_200; // OK
      }
    }
    else {
      $depth = ($options['depth'] == 'infinity' ? -1 : (int)$options['depth']);
      $is_writelock = ($options['type'] == 'write');
      $is_exclusive = ($options['scope'] == 'exclusive');
      if (dav_obtain_lock($resource, $options['locktoken'], $options['timeout'], $is_writelock, $is_exclusive, $depth)) {
        return STATUS_200; // OK
      }
    }

    return STATUS_409; // Conflict
  }

  /**
   * Implementation of the DAV UNLOCK method.
   *
   * @see http://www.webdav.org/specs/rfc2518.html#METHOD_UNLOCK
   */
  function UNLOCK(&$options) {
    _dav_trace('unlock', $options);

    $path = _dav_explode_path($options['path']);
    if (($resource = _dav_resolve($path))) {
      if (dav_release_lock($resource, $options['token'])) {
        return STATUS_204; // No Content
      }
    }

    return STATUS_409; // Conflict
  }
}

//////////////////////////////////////////////////////////////////////////////
// DAV API

/**
 * Looks up a DAV resource of the given name in the given path.
 */
function dav_lookup($parent, $name, $path = NULL) {
  foreach (_dav_implements('lookup') as $module) {
    if (($entry = _dav_invoke($module, 'lookup', $parent, $name, $path)) !== NULL) {
      return $entry;
    }
  }
}

/**
 * Returns a list of all DAV resources in the given path.
 */
function dav_list($parent, $path = NULL) {
  return _dav_invoke_all('list', $parent, $path);
}

/**
 * Retrieves properties for a DAV resource.
 *
 * This assumes the resource exists, i.e. it has previously been looked up
 * via dav_lookup(), for instance.
 */
function dav_propfind($resource) {
  return _dav_invoke_all('propfind', $resource);
  //$args = func_get_args();
  //array_unshift($args, 'propfind');
  //return call_user_func_array('_dav_invoke_all', $args);
}

//////////////////////////////////////////////////////////////////////////////
// DAV helpers

/**
 * Invokes a DAV hook in all modules which implement the DAV API.
 */
function _dav_dispatch($hook) {
  $args = func_get_args();
  //_dav_trace('dispatch', $args); // DEBUG

  foreach (_dav_implements($hook, array_slice($args, 1)) as $module) {
    $func_args = array_merge(array($module), $args);
    if (($result = call_user_func_array('_dav_invoke', $func_args)) !== NULL)
      return $result;
  }

  return NULL; // no handler
}

/**
 * Determines which modules implement a given DAV hook.
 *
 * @param $hook
 *   The name of the hook.
 * @return
 *   An array with the names of the modules which implement this hook.
 */
function _dav_implements($hook, array $args = array()) {
  $modules = module_implements('dav_'. $hook);
  $prepend = array('dav');

  // HACK: This is a special case for MKCOL/PUT requests in the root
  // collection. It's needed to prevent ambiguity and to let the
  // administrator select which module should handle these requests (see
  // the option 'Root collection owner' at admin/settings/dav).
  if (($hook == 'mkcol' || $hook == 'put') && module_exists(DAV_ROOT_MODULE)) {
    if (isset($args[0][0]) && $args[0][0] == ROOT_COLLECTION) {
      $prepend = array_merge($prepend, array(DAV_ROOT_MODULE));
    }
  }

  // Make sure the DAV core is the first in the list, while preserving any
  // potentially customized load order the other modules may have:
  return array_unique(array_merge($prepend, $modules));
}

/**
 * Invokes a given DAV hook in a particular module.
 *
 * @param $module
 *   The name of the module.
 * @param $hook
 *   The name of the hook to invoke.
 * @param ...
 *   Arguments to pass to the hook implementation.
 * @return
 *   The return value of the hook implementation.
 */
function _dav_invoke($module, $hook) {
  $hook = 'dav_'. $hook;
  if (module_hook($module, $hook)) {
    $args = func_get_args();
    $function = $module .'_'. $hook;
    return call_user_func_array($function, array_slice($args, 2));
  }
}

/**
 * Invokes a given DAV hook in all enabled modules that implement it.
 *
 * @param $hook
 *   The name of the hook to invoke.
 * @param ...
 *   Arguments to pass to the hook.
 * @return
 *   TODO
 */
function _dav_invoke_all($hook) {
  $args = func_get_args();
  $hook = array_shift($args);
  $return = array();
  foreach (_dav_implements($hook, $args) as $module) {
    $function = $module .'_dav_'. $hook;
    $result = call_user_func_array($function, $args);
    if (is_array($result)) {
      // Not using array_merge() here as it renumbers numerical keys
      $return += $result;
    }
  }
  return $return;
}

/**
 *
 */
function _dav_resolve($path) {
  $parent = array(NULL, NULL);
  foreach (array_values($path) as $depth => $name) {
    if (!($entry = dav_lookup($parent, $name, array_slice($path, 0, $depth))))
      return FALSE;
    $parent = $entry;
  }
  return $parent;
}

/**
 *
 */
function _dav_resources($path, $parent, $depth = 0) {
  $files = array();
  // Current directory
  $files[] = array('path' => _dav_urlencode(_dav_implode_path($path)), 'props' => _dav_mkprops($parent));
  // Subdirectories and resources
  if (!empty($depth)) {
    foreach (dav_list($parent, $path) as $name => $entry) {
      $files[] = array('path' => _dav_urlencode(_dav_implode_path($path, $name)), 'props' => _dav_mkprops($entry));
    }
  }
  return $files;
}

function _dav_implode_path($path, $name = NULL) {
  return (empty($path) ? '/' : '/'. implode('/', $path) .'/') . ($name ? $name : '');
}

function _dav_explode_path($path) {
  return ($path == '/') ? array() : array_slice(explode('/', $path), 1);
}

function _dav_urlencode($url) {
  return HTTP_WebDAV_Server::_urlencode($url);
}

function _dav_tmpname() {
  return tempnam(file_directory_temp(), 'drupal_dav_');
}

function _dav_mkprops($resource) {
  $props = dav_propfind($resource);
  return array_map('_dav_mkprop', array_keys($props), array_values($props));
}

function _dav_mkprop($x, $y) {
  return array('ns' => 'DAV:', 'name' => $x, 'val' => $y);
}

function _dav_trace($method, $options = NULL) {
  if (DAV_TRACE) {
    if (module_exists('trace')) {
      $msg = strtoupper($method) .'('. trace_format_php($options) .');';
      trace('dav', $msg);
    }
    else {
      //@include_once(dirname(__FILE__) .'/debug.inc'); // DEBUG
      //dump($method, $options); // DEBUG
    }
  }
}

//////////////////////////////////////////////////////////////////////////////
// DAV CRUD helpers

function dav_intern_resource($resource, $module = '', $auto_create = TRUE) {
  list($type, $key) = $resource;

  // Shortcut for the root collection, which doesn't need to be stored anywhere
  if ($type === DAV_ROOT_COLLECTION)
    return 0;

  $result = db_query("SELECT r.id FROM {dav_resources} r WHERE r.type = '%s' AND r.key = '%s'", $type, $key);
  if (($row = db_fetch_object($result))) {
    return $row->id;
  }
  else if ($auto_create) {
    $resource_id = db_next_id('{dav_resources}_id');
    db_query("INSERT INTO {dav_resources} (id, type, `key`, module) VALUES (%d, '%s', '%s', '%s')", $resource_id, $type, $key, $module);
    return $resource_id;
  }
}

function dav_obtain_lock($resource, $token, $timeout, $is_writelock = TRUE, $is_exclusive = FALSE, $depth = 0) {
  global $user;
  if (($resource_id = dav_intern_resource($resource))) {
    db_query("INSERT INTO {dav_locks} (id, resource_id, user_id, token, depth, is_writelock, is_exclusive, created_at, updated_at, expires_at) VALUES (%d, %d, %d, '%s', %d, %d, %d, %d, %d, %d)", db_next_id('{dav_locks}_id'), $resource_id, $user->uid, $token, $depth, $is_writelock, $is_exclusive, time(), time(), $timeout);
    return TRUE; // TODO: check affected rows
  }
}

function dav_renew_lock($resource, $token, $timeout) {
  global $user;
  if (($resource_id = dav_intern_resource($resource))) {
    db_query("UPDATE {dav_locks} SET expires_at = %d, updated_at = %d WHERE resource_id = %d AND token = '%s'", $timeout, time(), $resource_id, $token);
    return TRUE; // TODO: check affected rows
  }
}

function dav_release_lock($resource, $token) {
  if (($resource_id = dav_intern_resource($resource))) {
    db_query("DELETE FROM {dav_locks} WHERE resource_id = %d AND token = '%s'", $resource_id, $token);
    return TRUE; // TODO: check affected rows
  }
}

function dav_update_property($resource, $namespace, $name, $value) {
  if (($resource_id = dav_intern_resource($resource))) {
    return db_query("REPLACE INTO {dav_properties} SET resource_id = %d, namespace = '%s', name = '%s', value = '%s'", $resource_id, $namespace, $name, $value);
  }
}

function dav_delete_property($resource, $namespace, $name) {
  if (($resource_id = dav_intern_resource($resource))) {
    return db_query("DELETE FROM {dav_properties} WHERE resource_id = %d AND namespace = '%s' AND name = '%s'", $resource_id, $namespace, $name);
  }
}
