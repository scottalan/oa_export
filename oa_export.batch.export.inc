<?php

require_once 'oa_export.entity.export.inc';
require_once 'oa_export.fields.export.inc';

define('BLUEPRINT_SPACE', 'field_oa_clone_space');

/**
 * Prepare the export and create a batch process.
 *
 * @param object $blueprint
 *   The fully loaded blueprint entity.
 * @param string $export_type
 *   Defaults to 'file' but could also be 'module'.
 */
function oa_export_batch_export($blueprint, $export_type = 'file') {

  switch ($export_type) {
    case 'file':
      $_SESSION['oa_export'] = array();

      // Store the path we will link to after download.
      $_SESSION['oa_export']['redirect'] = 'admin/structure/taxonomy/space_type';

      // Store the export directory in the session variable.
      if ($_SESSION['oa_export']['directory'] = oa_export_create_temp_export_directory($blueprint->name)) {
        // Setup a directory for exporting files.
        if (oa_export_create_directories($_SESSION['oa_export']['directory'] . '/files')) {
          $_SESSION['oa_export']['files_directory'] = $_SESSION['oa_export']['directory'] . '/files';
        }
        else {
          drupal_set_message(t('Could not create @files', array('@files' => $_SESSION['oa_export']['directory'] . '/files')), 'error');
          oa_export_cleanup($_SESSION['oa_export']['directory'], $_SESSION['oa_export']['redirect']);
        }

        // Temporary directory for system.
        $tmp_dir = sys_get_temp_dir();
        if (!is_writable($tmp_dir)) {
          drupal_set_message(t('In order for this export to work "%dir" needs to be writable.', array('%dir' => $tmp_dir)), 'error');
          oa_export_cleanup($_SESSION['oa_export']['directory'], $_SESSION['oa_export']['redirect']);
        }

        $batch = array(
          'title' => t('Blueprint Download'),
          'init_message' => t('Preparing to download "!name" blueprint', array('!name' => $blueprint->name)),
          'finished' => 'oa_export_batch_export_download_finished',
        );

        // Defines batch operations for the batch.
        oa_export_batch_export_operations($batch, $blueprint);

        batch_set($batch);
        // Redirect the user to a page to download the file.
        batch_process('oa_export/download');
      }
      else {
        drupal_set_message(t('Could not create a directory in your system temporary directory.'), 'error');
        oa_export_cleanup($_SESSION['oa_export']['directory'], $_SESSION['oa_export']['redirect']);
      }
      break;

    case 'module':
      // Store the path we will link to after download.
      $_SESSION['oa_export']['redirect'] = 'blueprint/export/' . $blueprint->tid
      ;
      $batch = array(
        'title' => t('Blueprint Module Export'),
        'init_message' => t('Preparing to export the "!name" blueprint to a module.', array('!name' => $blueprint->name)),
        'finished' => 'oa_export_batch_export_module_finished',
      );

      // Defines batch operations for the batch.
      oa_export_batch_export_operations($batch, $blueprint);

      batch_set($batch);
      break;

    default:
      break;
  }
}

/**
 * Defines batch operations for the download batch.
 *
 * @param array $batch
 *   The current batch.
 * @param object $blueprint
 *   The fully loaded blueprint entity.
 */
function oa_export_batch_export_operations(&$batch, $blueprint) {

  // Get the entity reference of the space this blueprint is built on.
  $wrapper = entity_metadata_wrapper('taxonomy_term', $blueprint);
  $space = $wrapper->{BLUEPRINT_SPACE}->value();

  // The blueprint.
  $batch['operations'][] = array('_oa_export_batch_export_blueprint', array($blueprint, $space));

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
 * A batch operation to export a blueprint. We want to export the blueprint but don't want the parents of the space the
 * blueprint is being built on.
 *
 * @param object $blueprint
 *   The fully loaded blueprint entity.
 * @param object $space
 *   The space being exported.
 * @param array $context
 *   Passed around between batch operations.
 */
function _oa_export_batch_export_blueprint($blueprint, $space, &$context) {

  // As of now we only allow one blueprint export at a time.
  if (empty($context['sandbox']['max'])) {
    $context['sandbox']['progress'] = 0;
    $context['sandbox']['max'] = 1;
    $context['results']['export'] = array();
    $context['results']['messages'] = array();
    $context['results']['total'] = (!empty($context['results']['total']) ? $context['results']['total'] : 0) + $context['sandbox']['max'];
  }

  // If we have access to this session variable let's store it in the results, this is more stable.
  if (isset($_SESSION['oa_export']['module_path'])) {
    $context['results']['export_path'] = $_SESSION['oa_export']['module_path'];
  }
  if (isset($_SESSION['oa_export']['module'])) {
    $context['results']['module'] = $_SESSION['oa_export']['module'];
  }

  // Store the reference to the space the blueprint is referencing.
  $space_reference = $blueprint->field_oa_clone_space;

  // Remove the space reference for now so it doesn't get exported yet. We will export it next.
  $blueprint->field_oa_clone_space = array();

  // Export the blueprint.
  oa_export_entity_export('taxonomy_term', $blueprint, $results);

  // Put the reference to the space back so we can reference it during our import.
  $context['results']['export']['taxonomy_term:' . $blueprint->tid]->field_oa_clone_space = $space_reference;

  // Remove the oa_parent_space reference so we don't get this space's parent space and all of its dependencies.
  $space->oa_parent_space = array();

  // Export the space without reference to its parent space.
  oa_export_entity_export('node', $space, $context['results']);

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
