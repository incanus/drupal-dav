<?php
// $Id$

/**
 * @file
 * DAV API admin page callbacks.
 */

//////////////////////////////////////////////////////////////////////////////
// DAV API settings form

function dav_admin_settings() {
  $form = array();

  // DAV server settings
  $form['server'] = array('#type' => 'fieldset', '#title' => t('DAV server settings'), '#collapsible' => TRUE, '#collapsed' => TRUE);
  $form['server']['dav_server'] = array(
    '#type'          => 'radios',
    '#title'         => t('DAV server'),
    '#default_value' => (int)DAV_SERVER,
    '#options'       => array(FALSE => t('Disabled'), TRUE => t('Enabled')),
    '#description'   => t('Whether to enable the DAV server functionality provided by this module. Note that this requires the <a href="!url" target="_blank">PEAR HTTP_WebDAV_Server</a> library to be available.', array('!url' => DAV_PEAR_SERVER)),
  );
  $form['server']['dav_root_url'] = array(
    '#type'          => 'textfield',
    '#title'         => t('DAV URL'),
    '#default_value' => url(DAV_ROOT .'/', array('absolute' => TRUE)),
    '#size'          => 60,
    '#maxlength'     => 255,
    '#description'   => t('This is the absolute URL to the DAV root collection provided by this module, if the DAV server functionality has been enabled above. Use this path in your DAV client program to access the DAV resources exported by this Drupal site. Note that, at present, this path can\'t be aliased or changed.'),
    '#attributes'    => array('disabled' => 'disabled'),
  );
  $form['server']['dav_dot_files'] = array(
    '#type'          => 'radios',
    '#title'         => t('Allow hidden file/collection creation'),
    '#default_value' => (int)DAV_DOT_FILES,
    '#options'       => array(FALSE => t('Disabled'), TRUE => t('Enabled')),
    '#description'   => t('Whether to allow files and collections with a name beginning with a dot character (.) to be created. Files named this way are interpreted as hidden files on Unix systems. While this is enabled by default to provide maximum compatibility with various operating systems and clients, you should consider disabling it if you are sure that no useful information will be stored in hidden files. See also the Windows and Mac OS X compatibility sections, below.'),
  );
  $form['server']['dav_trace'] = array(
    '#type'          => 'radios',
    '#title'         => t('Enable DAV method tracing'),
    '#default_value' => (int)DAV_TRACE,
    '#options'       => array(FALSE => t('Disabled'), TRUE => t('Enabled')),
    '#description'   => t('Whether to log debug/troubleshooting information on all DAV HTTP methods. Note that enabling this will have a negative impact on performance and you should never use this except for troubleshooting purposes. <em>Requires trace.module.</em>'),
  );

  if (DAV_SERVER && !_dav_load_server()) {
    drupal_set_message(t('Warning: unable to load the <a href="!url" target="_blank">PEAR HTTP_WebDAV_Server</a> library.', array('!url' => DAV_PEAR_SERVER)), 'error');
  }

  // DAV server modules
  $form['modules'] = array('#type' => 'fieldset', '#title' => t('DAV server modules'), '#collapsible' => TRUE, '#collapsed' => TRUE);
  if (count(dav_get_modules()) > 0) {
    $header = array(t('Name'), t('Description'));
    $rows   = array();
    foreach (dav_get_modules('info') as $name => $info) {
      $rows[] = array($info->name, $info->description);
    }
    $modules = theme('table', $header, $rows);
  }
  else {
    $modules = '<div>'. t('No DAV modules currently installed; DAV server disabled.') .'</div>';
  }
  $form['modules']['dav_modules'] = array('#type' => 'markup', '#value' => $modules);
  if (count(dav_get_modules()) > 0) {
    $form['modules']['dav_root_module'] = array(
      '#type'          => 'select',
      '#title'         => t('Root collection owner'),
      '#default_value' => DAV_ROOT_MODULE,
      '#options'       => dav_get_modules('titles'),
      '#description'   => t('Select which module should have ownership of the file/collection creation process in the DAV root collection. While all DAV server modules may publish items in the root collection, to prevent ambiguity only one module can respond to top-level PUT/MKCOL requests.'),
    );
  }

  // DAV client settings
  $form['client'] = array('#type' => 'fieldset', '#title' => t('DAV client settings'), '#collapsible' => TRUE, '#collapsed' => TRUE);
  $form['client']['dav_client'] = array(
    '#type'          => 'radios',
    '#title'         => t('DAV client'),
    '#default_value' => (int)DAV_CLIENT,
    '#options'       => array(FALSE => t('Disabled'), TRUE => t('Enabled')),
    '#description'   => t('Whether to attempt loading the <a href="!url" target="_blank">PEAR HTTP_WebDAV_Client</a> library on Drupal initialization in order to provide <tt>webdav://</tt> and <tt>webdavs://</tt> stream wrappers for PHP, allowing the use of remote DAV resources using the regular file system functions in PHP. Disabled by default, enable it if you need it.', array('!url' => DAV_PEAR_CLIENT)),
  );

  if (DAV_CLIENT && !_dav_load_client()) {
    drupal_set_message(t('Warning: unable to load the <a href="!url" target="_blank">PEAR HTTP_WebDAV_Client</a> library.', array('!url' => DAV_PEAR_CLIENT)), 'error');
  }

  // Windows compatibility
  $form['windows'] = array('#type' => 'fieldset', '#title' => t('Windows compatibility'), '#collapsible' => TRUE, '#collapsed' => TRUE);
  $form['windows']['dav_windows_server_discovery'] = array(
    '#type'          => 'radios',
    '#title'         => t('Expect broken server discovery'),
    '#default_value' => (int)DAV_WINDOWS_SERVER_DISCOVERY,
    '#options'       => array(FALSE => t('Disabled'), TRUE => t('Enabled')),
    '#description'   => t('This is a workaround for the Windows XP and Windows Vista WebDAV redirector bug <a target="_blank" href="http://support.microsoft.com/?kbid=831805">KB831805</a>. A <a target="_blank" href="http://www.greenbytes.de/tech/webdav/webfolder-client-list.html#issue-options-req-against-root">similar bug</a> also exists in most versions of the Microsoft Web Folder Client. Leave this enabled if you need to serve any Windows clients.'),
  );
  $form['windows']['dav_windows_basic_auth'] = array(
    '#type'          => 'radios',
    '#title'         => t('Expect invalid FQDN user names'),
    '#default_value' => (int)DAV_WINDOWS_BASIC_AUTH,
    '#options'       => array(FALSE => t('Disabled'), TRUE => t('Enabled')),
    '#description'   => t('This is a workaround for the Windows XP WebDAV redirector bug <a target="_blank" href="http://support.microsoft.com/?kbid=315621">KB315621</a>. Leave this enabled if you need to serve Windows XP clients.'),
  );
  $form['windows']['dav_windows_no_thumbs_db'] = array(
    '#type'          => 'radios',
    '#title'         => t('Prevent thumbnail cache (Thumbs.db) creation'),
    '#default_value' => (int)DAV_WINDOWS_NO_THUMBS_DB,
    '#options'       => array(FALSE => t('Disabled'), TRUE => t('Enabled')),
    '#description'   => t('Disallows the creation of spurious <a target="_blank" href="http://en.wikipedia.org/wiki/Thumbs.db">Thumbs.db</a> files by Windows clients. Leaving this enabled is highly recommended, but a yet better option in some cases may be to <a target="_blank" href="http://en.wikipedia.org/wiki/Thumbs.db#External_links">disable Thumbs.db creation</a> on the client side altogether.'),
  );

  // Mac OS X compatibility
  $form['macosx'] = array('#type' => 'fieldset', '#title' => t('Mac OS X compatibility'), '#collapsible' => TRUE, '#collapsed' => TRUE);
  $form['macosx']['dav_macosx_no_ds_store'] = array(
    '#type'          => 'radios',
    '#title'         => t('Prevent .DS_Store file creation'),
    '#default_value' => (int)DAV_MACOSX_NO_DS_STORE,
    '#options'       => array(FALSE => t('Disabled'), TRUE => t('Enabled')),
    '#description'   => t('Disallows the creation of spurious <a target="_blank" href="http://en.wikipedia.org/wiki/.DS_Store">.DS_Store</a> files by Mac OS X clients. Leaving this enabled is highly recommended, but a yet better option is to <a target="_blank" href="http://docs.info.apple.com/article.html?artnum=301711">disable .DS_Store creation over network connections</a> on the client side altogether.'),
  );
  $form['macosx']['dav_macosx_no_forks'] = array(
    '#type'          => 'radios',
    '#title'         => t('Prevent resource fork creation'),
    '#default_value' => (int)DAV_MACOSX_NO_FORKS,
    '#options'       => array(FALSE => t('Disabled'), TRUE => t('Enabled')),
    '#description'   => t('Disallows the creation of resouce forks, also known as dot-underscore files because their names start with "._". Unless you know what you are doing and grok what this is all about, it is recommended that you leave this setting enabled.'),
  );

  return system_settings_form($form);
}
