<?php
/**
 * @file oa_export.form.import.inc
 *
 * The form used for importing a blueprint.
 */

require_once 'oa_export.import.inc';
require_once 'formats/json.inc';

/**
 * The blueprint import form.
 */
function oa_export_blueprint_import_form($form, &$form_state, $action) {

  $form = array();
  $form['#attributes'] = array(
    'enctype' => "multipart/form-data",
  );
  $form['#prefix'] = "<p>";
  $form['#prefix'] .= t('You may import your blueprint by uploading the file you exported.') . " ";
  $form['#prefix'] .= "</p>";

  $form['upload'] = array(
    '#type' => 'fieldset',
    '#title' => t('Upload file'),
    '#collapsible' => FALSE,
  );
  $form['upload']['file'] = array(
    '#type' => 'file',
    '#description' => t('To clear this field, <a href="!reset">reset the form</a>.', array('!reset' => url($_GET['q'])))
  );

  $form['actions'] = array('#type' => 'actions');
  $form['actions']['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Import'),
  );

  return $form;
}

/**
 * Validate function for the blueprint import form.
 */
function oa_export_blueprint_import_form_validate($form, &$form_state) {
  if (!$_FILES['files']['name']['file']) {
    drupal_set_message(t('Please upload a file to import.'), 'error');
    form_set_error('', NULL);
  }
}

/**
 * Submit function for the blueprint import form.
 */
function oa_export_blueprint_import_form_submit($form, &$form_state) {

  $original = $_FILES['files']['name']['file'];
  // Save the uploaded file.
  $file = file_save_upload('file', array('file_validate_extensions' => array()), FALSE, FILE_EXISTS_REPLACE);
  if (!$file) {
    drupal_set_message(t('Error: Node export could not save file.'), 'error');
  }
  else {
    $file->original = $original;
    // Set the form value for the file.
    form_set_value($form['upload']['file'], serialize($file), $form_state);
  }
  // Make sure we have a value for the file.
  if ($form_state['values']['file'] && isset($file)) {

    // The temporary directory to extract files to.
    $directory = file_directory_temp();

    // Get a new archiver.
    $archiver = archiver_get_archiver($file->uri);
    if (!$archiver) {
      throw new Exception(t('Cannot extract %archive, not a valid archive.', array ('%archive' => $file->filename)));
    }

    // Remove the directory if it exists, otherwise it might contain a mixture of
    // old files mixed with the new files (e.g. in cases where files were removed
    // from a later release).
    $files = $archiver->listContents();

    // Unfortunately, we can only use the directory name to determine the project
    // name. Some archivers list the first file as the directory (i.e., MODULE/)
    // and others list an actual file (i.e., MODULE/README.TXT).
    $project = strtok($files[0], '/\\');

    $extract_location = $directory . '/' . $project;
    if (file_exists($extract_location)) {
      file_unmanaged_delete_recursive($extract_location);
    }

    $archiver->extract($directory);

    // Store the location we will be extracting the file contents to.
    $_SESSION['oa_export']['extract_location'] = $extract_location;

    // Build the batch process to import our file.
    oa_export_batch_import($files, $extract_location);
    // Start the batch process.
    batch_process();
  }
  else {
    drupal_set_message(t("There was an error importing the file, try exporting and re-importing."));
  }

  // We need to send this user somewhere, and we know they have permission
  // for this page:
  drupal_goto('blueprint/import');
}
