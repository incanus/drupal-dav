<?php
// $Id$

/**
 * @file
 * DAV FS admin page callbacks.
 */

//////////////////////////////////////////////////////////////////////////////
// DAV FS settings form

function dav_fs_admin_settings() {
  $form = array();

  // If no setting has been saved yet, and the directory doesn't exist, show a prominent warning.
  if (is_null(variable_get('dav_fs_root', NULL)) && !is_dir(DAV_FS_ROOT)) {
    form_set_error('dav_fs_root', t('The directory %directory doesn\'t exist yet. It will be created when you save these settings.', array('%directory' => DAV_FS_ROOT)));
  }

  // File system settings
  $form['server'] = array('#type' => 'fieldset', '#title' => t('File system settings'), '#collapsible' => TRUE, '#collapsed' => FALSE);
  $form['server']['dav_fs_root'] = array(
    '#type'          => 'textfield',
    '#title'         => t('DAV file system root'),
    '#default_value' => DAV_FS_ROOT,
    '#size'          => 60,
    '#maxlength'     => 255,
    '#description'   => t('A file system path which will be used as the DAV root directory. The directory should exist and be writable by Drupal. If it doesn\'t exist yet, Drupal will attempt to create it.'),
  );

  return system_settings_form($form);
}

function dav_fs_admin_settings_validate($form, &$form_state) {
  extract($form_state['values'], EXTR_SKIP | EXTR_REFS);

  $dav_fs_root = rtrim($dav_fs_root, '/\\');
  file_check_directory($dav_fs_root, TRUE, 'dav_fs_root');
}
