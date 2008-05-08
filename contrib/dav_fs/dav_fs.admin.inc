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

  // File system settings
  $form['server'] = array('#type' => 'fieldset', '#title' => t('File system settings'), '#collapsible' => TRUE, '#collapsed' => TRUE);
  $form['server']['dav_fs_root'] = array(
    '#type'          => 'textfield',
    '#title'         => t('DAV file system root'),
    '#default_value' => DAV_FS_ROOT,
    '#size'          => 60,
    '#maxlength'     => 255,
    '#description'   => t('A file system path which will be used as the DAV root directory. The directory must exist and be writable by Drupal.'),
  );

  return system_settings_form($form);
}

function dav_fs_admin_settings_validate($form, &$form_state) {
  extract($form_state['values'], EXTR_SKIP | EXTR_REFS);

  file_check_directory($dav_fs_root, TRUE, 'dav_fs_root');
}
