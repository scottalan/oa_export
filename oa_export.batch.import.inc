<?php

require_once 'oa_export.utils.inc';
require_once 'oa_export.import.inc';

/**
 * Extract the file being imported.
 *
 * @param string $archive
 *   The path to the archive that is being imported.
 * @param array $context
 *   The context of the batch.
 * @throws Exception
 *
 * @see update_manager_archive_extract().
 */
function oa_export_extract_archive($archive, &$context) {

  if (empty($context['sandbox']['max'])) {
    $context['sandbox']['progress'] = 0;
    $context['sandbox']['max'] = 1;
    $context['results']['total'] = (!empty($context['results']['total']) ? $context['results']['total'] : 0) + $context['sandbox']['max'];
  }

  // The temporary directory to extract files to.
  $directory = file_directory_temp();

  $archiver = archiver_get_archiver($archive->uri);
  if (!$archiver) {
    throw new Exception(t('Cannot extract %archive, not a valid archive.', array ('%archive' => $archive)));
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

  // Store some results for the next operation.
  $context['results']['files'] = $files;
  $context['results']['directory'] = $extract_location;

  $_SESSION['oa_export']['results'] = $context['results'];

  // Bump the progress indicator.
  $context['sandbox']['progress']++;

  // We are finished with a single space.
  $context['finished'] = $context['sandbox']['progress'] / $context['sandbox']['max'];
}

/**
 * Finish function for extracting the archive.
 *
 * @param $success
 * @param $results
 * @param $operations
 */
function oa_export_extract_archive_finished($success, $results, $operations) {

  if ($success) {
    drupal_set_message(t('Successfully extracted archive'), 'status');
  }
  else {
    drupal_set_message(t('Error extracting archive.'), 'error');
  }
}

function oa_export_decode_archive_data_finished($success, $results, $operations) {

  if ($success) {
    drupal_set_message(t('Successfully decoded data from archive!'), 'status');
  }
  else {
    drupal_set_message(t('Error decoding data from archive.'), 'error');
  }
}

function oa_export_batch_import_space($import, &$context) {

  if (empty($context['sandbox']['max'])) {
    $context['sandbox']['progress'] = 0;
    $context['sandbox']['max'] = 1;
    $context['results']['total'] = (!empty($context['results']['total']) ? $context['results']['total'] : 0) + $context['sandbox']['max'];
  }

  // This is the space that we want to import.
  $original_space = $import['entity'];

  // See if we are importing a subspace.
  $parent_space = !empty($original_space->oa_parent_space) ? $_SESSION['oa_export']['space'] : NULL;

  // Prepare the node for import. This will call hook_clone_node_alter() so we
  // can make this just a simple node with the basics. We can then save that
  // node and get the new nid that we can then node_load(), alter by possibly
  // merging the $original_space with the one we loaded. Updating fields,
  // paragraphs, files, comments, etc. then re-saving the node a second time?

  // This probably needs to be done for spaces, sections, section_content, etc.
  // They need to exist in the system first, then reload them and alter them and
  // re-save them.

  $cloned_node = oa_clone_prepare($original_space, $parent_space, NULL, TRUE, $import); // Try without last TRUE

  // Need to unset this so oa_clone_node_insert() falls through.
  unset($cloned_node->clone_from_original_nid);
  // Save the new cloned (clean) node.
  node_save($cloned_node);

  // Now we need to look for things in the original node that we need to import
  // and then relate them to the new node.
  oa_export_import_entity_data($import, $cloned_node, $original_space);

  // Set a message.
  $context['message'] = t('Preparing to import Space: !title', array('!title' => $cloned_node->title));

  $context['results']['messages'][] = t('The Space: !title was imported.', array('!title' => $cloned_node->title));

  // Bump the progress indicator.
  $context['sandbox']['progress']++;

  // We are finished with a single space.
  $context['finished'] = $context['sandbox']['progress'] / $context['sandbox']['max'];
}

function oa_export_batch_import_section($import, $section, $section_id, &$context) {

  if (empty($context['sandbox']['max'])) {
    $context['sandbox']['progress'] = 0;
    $context['sandbox']['max'] = count($section_id);
    $context['results']['total'] = (!empty($context['results']['total']) ? $context['results']['total'] : 0) + $context['sandbox']['max'];
  }

  // Create a new section node associating it with the correct space.
  $cloned_node = oa_clone_prepare($section, $_SESSION['oa_export']['space'], NULL, TRUE, $import['content']['section:' . $section_id]);

  // Save the new cloned (clean) node.
  unset($cloned_node->clone_from_original_nid);
  node_save($cloned_node);

  $_SESSION['oa_export']['section'] = $cloned_node->nid;

  // This is the section data we are targeting.
  $this_import = $import['content']['section:' . $section_id];

  // Now we need to look for things like (paragraphs, comments) in the original
  // node that may need to be imported and then relate it to the new node.
  oa_export_import_entity_data($this_import, $cloned_node, $section);

  // This should just be added as a helper function that the $import can be passed to.
  // It would be passed by reference so once this item has been completed it can
  // be removed from the import array.

  $vars = array(
    'id' => $section_id,
  );

  // This looks for stored import data for this section. For example, while
  // creating the space there might have been a paragraph_content item that
  // relates to this section. Now that the section is imported we can safely
  // create the paragraph item that relates to this section.
  oa_export_import_stored_data($import, $context, 'section:' . $section_id, $vars);

  // Bump the progress indicator.
  $context['sandbox']['progress']++;

  // We are finished with a single space.
  $context['finished'] = $context['sandbox']['progress'] / $context['sandbox']['max'];
}

function oa_export_batch_import_section_content($import, $content, &$context) {

  if (empty($context['sandbox']['max'])) {
    $context['sandbox']['progress'] = 0;
    $context['sandbox']['max'] = 1;
    $context['results']['total'] = (!empty($context['results']['total']) ? $context['results']['total'] : 0) + $context['sandbox']['max'];
  }

  $section_id = $_SESSION['oa_export']['section'];

  // Create a new section node associating it with the correct space.
  $cloned_node = oa_clone_prepare($content, $_SESSION['oa_export']['space'], $section_id, TRUE, $import['content']['section:' . $section_id]['content']['section_content:' . $content->nid]);

  // Save the new cloned (clean) node.
  unset($cloned_node->clone_from_original_nid);
  node_save($cloned_node);

  // This is the section we are targeting.
  $content_import = $import['content']['section:' . $section_id];

  // Now we need to look for things like (paragraphs, comments) in the original
  // node that may need to be imported and then relate it to the new node.
  oa_export_import_entity_data($content_import, $cloned_node, $content);

  // This looks for stored import data for this section. For example, while
  // creating the space there might have been a paragraph_content item that
  // relates to this section. Now that the section is imported we can safely
  // create the paragraph item that relates to this section.
  oa_export_import_stored_data($import, $context, 'stored_sections', $vars);

  // Bump the progress indicator.
  $context['sandbox']['progress']++;

  // We are finished with a single space.
  $context['finished'] = $context['sandbox']['progress'] / $context['sandbox']['max'];
}

/**
 * Import the blueprint.
 *
 * @param array $context
 *   The context of the batch.
 */
function oa_export_batch_import_blueprint($blueprint, &$context) {

  if (empty($context['sandbox']['max'])) {
    $context['sandbox']['progress'] = 0;
    $context['sandbox']['max'] = 1;
    $context['results']['total'] = (!empty($context['results']['total']) ? $context['results']['total'] : 0) + $context['sandbox']['max'];
  }

  // This is the new space nid we need to set for the blueprint.
  $blueprint_space_nid = $_SESSION['oa_export']['space'];

  // Unset the tid of this blueprint so it will generate a new one when
  // it is imported.
  $blueprint->tid = NULL;
  // Make sure we have the correct vocabulary vid.
  $vocab = taxonomy_vocabulary_machine_name_load('space_type');
  // Associate this taxonomy term with the correct vocabulary.
  $blueprint->vid = $vocab->vid;
  // Set the space reference to the nid of the space we just imported.
  $blueprint->field_oa_clone_space[LANGUAGE_NONE][0]['target_id'] = $blueprint_space_nid;
  // Save the blueprint.
  taxonomy_term_save($blueprint);

  // Set a message.
  $context['message'] = t('Preparing to import Blueprint: !title', array('!title' => $blueprint->title));

  $context['results']['messages'][] = t('Blueprint: !title was imported.', array('!title' => $blueprint->title));

  // Bump the progress indicator.
  $context['sandbox']['progress']++;

  // We are finished with a single space.
  $context['finished'] = $context['sandbox']['progress'] / $context['sandbox']['max'];
}

function oa_export_import_batch_finished($success, $results, $operations) {
  if ($success && isset($results['messages'])) {
    foreach ($results['messages'] as $message) {
      drupal_set_message($message, 'status');
    }
  }
}
