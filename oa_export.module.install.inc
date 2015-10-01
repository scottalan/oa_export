<?php
/**
 * @file oa_export.module.install.inc
 * Supports writing a new or updating an existing MODULE.install file.
 */

/**
 * Builds a schema version for an update hook written to a new MODULE.install file.
 *
 * @param int $version
 *   An existing schema version if one exists.
 *
 * @return string
 *   The schema version for an update hook in a new MODULE.install file.
 */
function oa_export_get_new_schema_version($version = 0) {
  if ($version > 0) {
    return $version + 1;
  }
  list($core) = explode('.', DRUPAL_CORE_COMPATIBILITY);
  // The schema version to use for hook_update_N().
  return $core . '000';
}

/**
 * Creates a new install file for a module.
 *
 * @param string $module_path
 *   The absolute path to the modules location.
 * @param $form_state
 *   A keyed array containing the current state of the form.
 *
 * @return bool
 *   Whether the file was successfully created or not.
 */
function oa_export_create_new_install_file($module_path, $form_state) {
  $module_name = $form_state['values']['machine_name'];
  // The schema version to use for hook_update_N().
  $version = oa_export_get_new_schema_version();
  // Write an implementation of hook_install().
  if ($success = oa_export_write_hook_install($module_path, $module_name, $version)) {
    // Write an implementation of hook_update_N().
    $success = oa_export_write_update_hook($module_path, $module_name, $version);
  }
  return $success;
}

/**
 * Reads through an existing install file so we better know how to write our changes to it.
 *
 * @param string $module_path
 *   The absolute path to the modules location.
 * @param $form_state
 *   A keyed array containing the current state of the form.
 *
 * @return bool
 *    Whether the file was successfully written to..
 */
function oa_export_update_existing_install_file($module_path, $form_state) {
  $module_name = $form_state['values']['generate_path'];
  $file = $module_path . '/' . $module_name . '.install';
  // We need to find out if there are any existing update hooks.
  // Find out what update, if any, the module has run so we can get the next hook_update_N() version.
  include_once(DRUPAL_ROOT . '/includes/install.inc');
  $schema_version = drupal_get_installed_schema_version($module_name);

  // Check first to see if the module has an install file. It may not exist or just be an empty file.
  if (!file_exists($file) || filesize($file) == 0) {
    oa_export_create_new_install_file($module_path, $form_state);
  }
  // The file exists and isn't empty.
  else {
    // Pattern used to find hook_install().
    $pattern = "/function " . $module_name . "_install\(\) {/";
    // Search the install file for an implementation of hook_install().
    if ($result = oa_export_search_file($file, $pattern)) {
      // Check to see the schema version is greater than 0. This lets us know if the module is enabled and the install
      // file contains no previous update hooks. We can just add a new update hook.
      if ($schema_version > SCHEMA_INSTALLED) {
        $new_version = oa_export_get_new_schema_version();
        return oa_export_write_update_hook($module_path, $module_name, $new_version);
      }
    }
    else {
      // We didn't find hook_install(). Let's check for existing updates.
      // Pattern used to find implementations of hook_update_N(). Using a pattern here instead of relying on what Drupal
      // found for a schema version in case the module isn't enabled yet.
      $pattern = "/function " . $module_name . "_update_([0-9]{4})\(\) {/";
      // Search the install file for implementations of hook_update_N().
      if ($results = oa_export_search_file($file, $pattern)) {
        // Get the latest version returned from the file.
        $latest_version = end($results);
        // Get the new version.
        $new_version = oa_export_get_new_schema_version($latest_version);
        // Write the update hook to the install file.
        return oa_export_write_update_hook($module_path, $module_name, $new_version);
      }
    }
    return TRUE;
  }
}

/**
 * Helper function to create the actual MODULE.install file.
 *
 * @param string $module_path
 *   The absolute path to the modules location.
 * @param array $module_name
 *   The machine_name of the module.
 * @param int $version
 *
 * @return bool
 *   Whether the file contents were successfully written or not.
 */
function oa_export_write_hook_install($module_path, $module_name, $version) {
  $output = '';
  $output .= "<?php\n/**\n* @file\n* Install, update and uninstall functions.\n*/\n";
  $output .= "\n";
  $output .= "/**\n* Implements hook_install().\n*/\n";
  $output .= "function {$module_name}_install() {\n";
  $output .= "  // Call our update hook.\n";
  $output .= "  {$module_name}_update_{$version}();\n";
  $output .= "}\n";

  if (file_put_contents($module_path . '/' . $module_name . '.install', $output, FILE_APPEND) === FALSE) {
    return FALSE;
  }
  else {
    return TRUE;
  }
}

/**
 * Writes an update hook for the MODULE.install file.
 *
 * @param string $module_path
 *   The absolute path to the modules location.
 * @param array $module_name
 *   The machine_name of the module.
 * @param $version
 *
 * @return bool
 *   Whether the file contents were successfully written or not.
 */
function oa_export_write_update_hook($module_path, $module_name, $version) {
  $output = "";
  $output .= "\n/**\n* Implements hook_update_N().\n* Imports a blueprint.\n*/\n";
  $output .= "function {$module_name}_update_{$version}() {\n";
  $output .= "  // Check for the oa_export directory.\n";
  $output .= "  if (file_exists('oa_export')) {\n";
  $output .= "    \$module = '" . $module_name . "';\n";
  $output .= "    // Get the full path to the module directory.\n";
  $output .= "    \$path = drupal_get_path('module', \$module) . '/' . OA_EXPORT_FILES;\n";
  $output .= "    // Start the import.\n";
  $output .= "    oa_export_batch_import(\$path, 'module');\n";
  $output .= "  }\n";
  $output .= "}\n";

  if (file_put_contents($module_path . '/' . $module_name . '.install', $output, FILE_APPEND) === FALSE) {
    return FALSE;
  }
  else {
    return TRUE;
  }
}
