<?php

require_once 'formats/json.inc';

/**
 * Creates a json representation of the entities.
 *
 * @param string $name
 *   The file name without the extension.
 * @param array $export
 *   Contains all entities that will be exported.
 * @param string $location
 *   The path the export will created at.
 * @return bool
 *   Lets us know the file was created.
 */
function oa_export_create_json_export($name, $export, $location) {
  // This is where the file will be created.
  $destination = file_destination($location . '/' . $name . '.json', FILE_EXISTS_RENAME);
  // Create a json object from our export.
  $json = oa_export_json_export($export);
  // Create the file.
  $file = file_put_contents($destination, $json);
  // Lets us know the file was created.
  if (!empty($file)) {
    return TRUE;
  }
  else {
    return FALSE;
  }
}

/**
 * Helper function to create a temporary export directory.
 *
 * @param string $name
 *  The name of the blueprint.
 * @return bool|string
 */
function oa_export_create_temp_export_directory($name) {
  // Format the name of the blueprint for the filename.
  $name = str_replace(' ', '_', strtolower($name));
  // The name of the directory we are storing the export.
  $dir_name = sys_get_temp_dir() . '/' . 'oa_export_' . $name . '_' . REQUEST_TIME;
  // Create our temporary directory.
  $created = @mkdir($dir_name, 0755, TRUE);
  if ($created) {
    // The name of the directory we will be placing data in for export.
    return $dir_name;
  }
  else {
    // Something happened and the directory couldn't be created.
    drupal_set_message(t('The @dir directory could not be created.', array('@dir' => $dir_name)), 'error');
    return FALSE;
  }

}

/**
 * Helper function to create directories.
 *
 * @param string $path
 *  The full path to the directory or directories you want to create.
 * @return bool|string
 */
function oa_export_create_directories($path) {
  $created = @mkdir($path, 0755, TRUE);
  if ($created) {
    return $created;
  }
  else {
    drupal_set_message(t('The path @path could not be created.', array('@path' => $path)));
    return FALSE;
  }

}

/**
 * Removes a directory recursively along with any files it may contain that was created during the export.
 *
 * @param string $dir
 *   The path to the directory.
 */
function oa_export_remove_directory($dir) {
  if (is_dir($dir)) {
    $objects = scandir($dir);
    foreach ($objects as $object) {
      if ($object != "." && $object != "..") {
        if (filetype($dir."/".$object) == "dir") {
          oa_export_remove_directory($dir."/".$object);
        }
        else {
          unlink($dir."/".$object);
        }
      }
    }
    reset($objects);
    @rmdir($dir);
  }
}
