<?php
// $Id$

/**
 * @file
 * PHPTemplate theme for displaying the index for a DAV collection.
 */

//////////////////////////////////////////////////////////////////////////////
// Theme constants

define('DAV_LS_FORMAT', '%12s  %-19s  %-s');

//////////////////////////////////////////////////////////////////////////////
// Theme implementation

/**
 * Renders a themed DAV collection index.
 *
 * @param $path
 * @param $resources
 *
 * @ingroup themeable
 */
function theme_dav_page($path, $resources = array()) {
  global $locale;
  $cwd = is_array($path) ? _dav_implode_path($path) : $path;
  $title = t('Index of %path', array('%path' => $cwd));

  ob_start();

  print '<?xml version="1.0" encoding="utf-8"?>' . "\n";
  print '<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN"' . "\n" .
    '  "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">' . "\n";
  print '<html xmlns="http://www.w3.org/1999/xhtml" xml:lang="' . $locale . '" lang="' . $locale . '">' . "\n";
  print "  <head>\n";
  print "    <title>$title</title>\n";
  print "  </head>\n";
  print "  <body>\n";
  print "    <h1>$title</h1>\n";
  print "    <pre>\n";
  print sprintf(DAV_LS_FORMAT, t('Size'), t('Last modified'), t('Name')) . "<hr />\n";

  foreach ($resources as $name => $resource) {
    print _dav_theme_resource($path, $name, $resource);
  }

  print "    </pre>\n";
  print "    <hr />\n";
  print '    ' . theme('dav_footer') . "\n";
  print "  </body>\n";
  print "</html>\n";

  return ob_get_clean();
}

/**
 * Renders a themed DAV resource for display in a collection index.
 *
 * @param $path
 * @param $name
 * @param $resource
 *
 * @ingroup themeable
 */
function theme_dav_resource($path, $name, $resource) {
  list($type, ) = $resource;
  $props = dav_propfind($resource);

  $size = !empty($props['getcontentlength']) ? format_size($props['getcontentlength']) : '-';
  $mtime = !empty($props['getlastmodified']) ? $props['getlastmodified'] : time();
  $mtime = format_date($mtime, 'small');
  $slash = ($props['resourcetype'] == 'collection') ? '/' : '';
  $name = check_plain($name);
  $link = '<a href="./' . $name . $slash . '" class="dav ' . $type . '">' . $name . '</a>' . $slash;

  return sprintf(DAV_LS_FORMAT . "\n", $size, $mtime, $link);
}

/**
 * Renders a themed footer used for displaying a DAV collection index.
 *
 * @ingroup themeable
 */
function theme_dav_footer() {
  return '<address>' . t('Powered by <a href="%url">Drupal</a>.', array('%url' => 'http://drupal.org/project/dav')) . '</address>';
}

//////////////////////////////////////////////////////////////////////////////
// Theme helpers

function _dav_theme_resource($path, $name, $resource) {
  list($type, $item) = $resource;

  $theme_registry = theme_get_registry();
  $theme_func = 'dav_' . $type;
  $theme_func = isset($theme_registry[$theme_func]) ? $theme_func : 'dav_resource';
  return theme($theme_func, $path, $name, $resource);
}
