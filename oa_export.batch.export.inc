<?php

require_once 'oa_export.entity.export.inc';
require_once 'oa_export.fields.export.inc';

define('BLUEPRINT_SPACE', 'field_oa_clone_space');

/**
 * Prepare the export and create a batch process.
 *
 * @param object $blueprint
 *   The fully loaded blueprint entity.
 */
function oa_export_batch_export($blueprint) {

  // Store space data in session.
  $_SESSION['oa_export'] = array();

  // Store the path we will link to after download.
  $_SESSION['oa_export']['redirect'] = 'admin/structure/taxonomy/space_type';

  // Store the export directory in the session variable.
  $_SESSION['oa_export']['directory'] = oa_export_create_temp_directory($blueprint->name);

  // Setup a directory for exporting files.
  $_SESSION['oa_export']['files_directory'] = oa_export_create_sub_directory($_SESSION['oa_export']['directory'], 'files');

  $batch = array(
    'title' => t('Blueprint Export'),
    'init_message' => t('Preparing to export !name blueprint', array('!name' => $blueprint->name)),
    'finished' => 'oa_export_batch_export_finished',
  );

  // Defines batch operations for the batch.
  oa_export_batch_export_operations($batch, $blueprint);

  // Temporary directory for system.
  $tmp_dir = sys_get_temp_dir();
  if (!is_writable($tmp_dir)) {
    drupal_set_message(t('In order for this export to work "%dir" needs to be writable.', array('%dir' => $tmp_dir)), 'error');
  }
  batch_set($batch);
  // Redirect the user to a page to download the file.
  batch_process('oa_export/download');
}

/**
 * Defines batch operations for the batch.
 *
 * @param array $batch
 *   The current batch.
 * @param object $blueprint
 *   The fully loaded blueprint entity.
 */
function oa_export_batch_export_operations(&$batch, $blueprint) {

  // The blueprint.
  $batch['operations'][] = array('_oa_export_batch_export_blueprint', array($blueprint));

  // Get the entity reference for the space, this blueprint is built on.
  $wrapper = entity_metadata_wrapper('taxonomy_term', $blueprint);
  $space = $wrapper->{BLUEPRINT_SPACE}->value();

  // The space should have been exported by the blueprint batch operation but the space can still have other
  // dependencies. e.g., comments.
  $batch['operations'][] = array('_oa_export_batch_export_dependency', array($space));

  // Entities that are sub groups of the space.
  $groups = oa_core_get_groups_by_parent($space->nid, NULL);

  // Iterate over the sub groups.
  foreach ($groups as $id) {
    // Load the sub group entity.
    $entity = entity_load_single('node', $id);

    // Define a batch operation to export the entity.
    $batch['operations'][] = array('_oa_export_batch_export_dependency', array($entity));
  }
}

/**
 * A batch operation to export a blueprint.
 *
 * @param object $blueprint
 *   The fully loaded blueprint entity.
 * @param array $context
 *   Passed around between batch operations.
 */
function _oa_export_batch_export_blueprint($blueprint, &$context) {

  // As of now we only allow one blueprint export at a time.
  if (empty($context['sandbox']['max'])) {
    $context['sandbox']['progress'] = 0;
    $context['sandbox']['max'] = 1;
    $context['results']['export'] = array();
    $context['results']['messages'] = array();
    $context['results']['total'] = (!empty($context['results']['total']) ? $context['results']['total'] : 0) + $context['sandbox']['max'];
  }

  // Export the entity.
  oa_export_entity_export('taxonomy_term', $blueprint, $context['results']);

  // Bump the progress indicator.
  $context['sandbox']['progress']++;

  // We are finished with a single space.
  $context['finished'] = $context['sandbox']['progress'] / $context['sandbox']['max'];
}

/**
 * Exports an entity that is a dependency for this blueprint.
 *
 * @param object $entity
 *   The entity that is being exported.
 * @param array $context
 *   Passed around between batch operations.
 */
function _oa_export_batch_export_dependency($entity, &$context) {

  // As of now we only allow one blueprint export at a time.
  if (empty($context['sandbox']['max'])) {
    $context['sandbox']['progress'] = 0;
    $context['sandbox']['max'] = 1;
    $context['results']['total'] = (!empty($context['results']['total']) ? $context['results']['total'] : 0) + $context['sandbox']['max'];
  }

  // Export the entity.
  oa_export_entity_export('node', $entity, $context['results']);

  // Bump the progress indicator.
  $context['sandbox']['progress']++;

  // We are finished with a single space.
  $context['finished'] = $context['sandbox']['progress'] / $context['sandbox']['max'];
}

/**
 * Finish function for the batch process.
 *
 * @param bool $success
 *   TRUE if the batch succeeds.
 * @param array $results
 *   Data stored in $context['results'] during the batch process.
 * @param $operations
 * @throws Exception
 */
function oa_export_batch_export_finished($success, $results, $operations) {
  if ($success) {

    try {
      // Generate a file name that matches our directory.
      $base = basename($_SESSION['oa_export']['directory']);
      $tmp = file_directory_temp();
      $_SESSION['oa_export']['file'] = $tmp . DIRECTORY_SEPARATOR . $base . '.tar.gz';

      // Create a json file to add to our export directory that contains the blueprint.
      $file = oa_export_create_json_export('entities', $results['export'], $_SESSION['oa_export']['directory']);

      if ($file) {
        // Build a compressed file from our blueprint.
        $new_tar = new Archive_Tar($_SESSION['oa_export']['file']);

        // We call this directly so we can modify the directory structure for our
        // tar file. This will remove "/tmp" from the tar.
        $new_tar->createModify(array($_SESSION['oa_export']['directory']), '', file_directory_temp());

        // Get the public file directory.
        $public = variable_get('file_public_path', conf_path() . '/files');

        // Copy the compressed file to a path we can download.
        if ($copy = file_unmanaged_copy($_SESSION['oa_export']['file'], $public, FILE_EXISTS_REPLACE)) {
          $_SESSION['oa_export']['download_path'] = $copy;
        }
        else {
          throw new Exception(t('There was a problem copying the export to !public', array('!public' => $public)));
        }

        // Display a message telling the user the export is done.
        drupal_set_message(t('Finished exporting your Blueprint. Check below for notices.'), 'status');
        foreach ($results['messages'] as $message) {
          // Messages should already be formatted correctly with t().
          drupal_set_message($message, 'warning');
        }
      }
      else {
        drupal_set_message(t('There was an error creating the export file.'), 'error');
      }
    }
    catch (Exception $e) {
      drupal_set_message(t('Error: %message', array('%message' => $e->getMessage())));
    }
  }
  else {
    throw new Exception(t('The batch export was unsuccessful'));
  }
}

/**
 * Helper function that handles the file download.
 */
function oa_export_batch_export_finished_redirect() {

  if (empty($_SESSION['oa_export']['directory']) || empty($_SESSION['oa_export']['file'])) {
    return t('The file cannot be found.');
  }

  // Redirect to the download file.
  $redirect = base_path() . $_SESSION['oa_export']['download_path'];
  drupal_add_js('setTimeout(function() { window.location.href = "' . $redirect . '"; }, 2000);', 'inline');

  // Remove the temporary directory we created.
  oa_export_remove_temp_directory($_SESSION['oa_export']['directory']);

  $output = theme('blueprint_download_page', array());

  // Remove the temporary tar file.
  unlink($_SESSION['oa_export']['file']);

  // Get rid of our session variables.
  unset($_SESSION['oa_export']);

  return $output;
}
