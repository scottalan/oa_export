<?php

require_once 'formats/json.inc';

function oa_export_new_file($filename) {
  $mime = 'application/x-gzip';
  header('Content-type: ' . $mime);
  header('Content-Disposition: attachment; filename="' . $filename . '"');
  $data = file_get_contents($filename);
  echo $data;
  // Clean exit.
  module_invoke_all('exit');
  exit;
}

function oa_export_create_json_export($type, $export, $location) {

  $destination = file_destination($location . '/' . $type . '.json', FILE_EXISTS_RENAME);

  // Create a json object from our export.
  $json = oa_export_json_export($export);

  $file = file_put_contents($destination, $json);

  if (!empty($file)) {
    return TRUE;
  }
  else {
    return FALSE;
  }
}

function oa_export_create_json_export_new($export, $location) {

  $time = REQUEST_TIME;
  $path = $location . '/oa_export_blueprint.json';

  $destination = file_destination($path, FILE_EXISTS_RENAME);

  // Create a json object from our export.
  $json = oa_export_json_export($export);

  $file = file_put_contents($destination, $json);

  if (!empty($file)) {
    return $path;
  }
  else {
    return FALSE;
  }
}

function oa_export_create_temp_directory() {

  // Create a timestamp for our directory name.
  $time = REQUEST_TIME;
  // Temporary directory for system.
  $tmp_dir = sys_get_temp_dir();
  // Create our temporary directory.
  $created = @mkdir($tmp_dir . '/' . 'oa_export_' . $time, 0755, TRUE);
  if ($created) {
    // The name of the directory we will be placing data in for export.
    return $tmp_dir . '/' . 'oa_export_' . $time;
  }
  else {
    // Something happened and the directory couldn't be created.
    return FALSE;
  }

}

function oa_export_create_sub_directory($parent, $child) {

  $new_directory = $parent . '/' . $child;
  // Create our child directory.
  $created = @mkdir($new_directory, 0755, TRUE);
  if ($created) {
    return $new_directory;
  }
  else {
    // Something happened and the directory couldn't be created.
    return FALSE;
  }

}

function oa_export_remove_temp_directory($dir) {
  if (is_dir($dir)) {
    $objects = scandir($dir);
    foreach ($objects as $object) {
      if ($object != "." && $object != "..") {
        if (filetype($dir."/".$object) == "dir") {
          rmdir($dir."/".$object);
        }
        else {
          unlink($dir."/".$object);
        }
      }
    }
    reset($objects);
    rmdir($dir);
  }
}
