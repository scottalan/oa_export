<?php
/**
 * @file oa_export.module.module.inc
 * Supports writing a new MODULE.module file.
 */

/**
 * Adds an empty module file when creating a new module.
 *
 * @param string $module_path
 *   The absolute path to the modules location.
 * @param $form_state
 *   A keyed array containing the current state of the form.
 *
 * @return bool
 */
function oa_export_create_new_module_file($module_path, $form_state) {
  $module_name = $form_state['values']['machine_name'];
  $file = $module_path . '/' . $module_name . '.module';
  // If the module doesn't exist we just add a comment to the .module file.
  $output = "<?php\n/**\n* @file\n* Drupal needs this blank file.\n*/\n";

  // Write to the file.
  if (file_put_contents($file, $output, FILE_APPEND) === FALSE) {
    return FALSE;
  }
  else {
    return TRUE;
  }
}

/**
 * This is here in case an empty module file exists.
 *
 * @param string $module_path
 *   The absolute path to the modules location.
 * @param $form_state
 *   A keyed array containing the current state of the form.
 *
 * @return bool
 */
function oa_export_update_existing_module_file($module_path, $form_state) {
  $module_name = $form_state['values']['machine_name'];
  $file = $module_path . '/' . $module_name . '.module';
  // Check first to see if the module has a module file. It may not exist or just be an empty file.
  if (!file_exists($file) || filesize($file) == 0) {
    oa_export_create_new_module_file($module_path, $form_state);
  }
  // The file exists and is not empty so just move on.
  else {
    return TRUE;
  }
}
