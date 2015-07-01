<?php

/**
 * @file
 * The OA export JSON format handler.
 *
 * Adds JSON format to OA export.
 */

/**
 * Export callback.
 */
function oa_export_json_export($export) {
  return drupal_json_encode(oa_export_json_encode_objects($export));
}

/**
 * Mark objects as being objects.
 */
function oa_export_json_encode_objects($var) {
  if (is_object($var)) {
    $var = (array) $var;
    $var['#oa_export_object'] = '1';
  }
  if (is_array($var)) {
    foreach ($var as $key => $value) {
      $var[$key] = oa_export_json_encode_objects($value);
    }
  }
  return $var;
}

/**
 * Import callback.
 */
function oa_export_json_import($code_string) {
  return oa_export_json_decode_objects(drupal_json_decode($code_string));
}

/**
 * Recursively convert arrays back to objects.
 */
function oa_export_json_decode_objects($array) {
  if (is_array($array)) {
    foreach ($array as $k => $v) {
      if (is_array($v)) {
        $array[$k] = oa_export_json_decode_objects($v);
      }
    }
    if (isset($array['#oa_export_object'])) {
      unset($array['#oa_export_object']);
      $array = (object) $array;
    }
    return $array;
  }
}
