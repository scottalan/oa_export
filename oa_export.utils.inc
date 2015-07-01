<?php

require_once 'oa_export.paragraphs.export.inc';

function oa_export_export_paragraph_items($items, $entity_type, $label, &$context) {

  $results = array();

  foreach ($items as $delta => $item) {

    // Load the paragraph item entity.
    $paragraph = paragraphs_field_get_entity($item);

    if (isset($paragraph)) {

      // Build an exportable paragraph item with what we need to import.
      $paragraph_export = array();
      $paragraph_export['field_name'] = $paragraph->field_name;
      $paragraph_export['bundle'] = $paragraph->bundle;

      // We also need our fields.
      $fields = field_info_instances('paragraphs_item', $paragraph->bundle);

      foreach ($fields as $field_name => $field) {
        $paragraph_export[$field_name] = $paragraph->{$field_name};

        // Information we want to pass to our alter hook.
        $data = array('field_name' => $field_name, 'paragraph' => $paragraph);

        // Allows custom export functions keyed on paragraph bundle to
        // accomplish tasks that may be needed when exporting this paragraph
        // item or add to the export.
        drupal_alter('oa_export_' . $paragraph->bundle, $paragraph_export, $data);

        // Add the final paragraph export to our results.
        $results[$paragraph->entityType() . ':' . $paragraph->item_id . ':' . $paragraph->hostEntityId()] = $paragraph_export;
      }
    }
  }

  return $results;
}

/**
 * Copies media (image) files for our export.
 *
 * @param string $field_name
 *   The type of entity, or bundle.
 * @param object $entity
 *   The full entity object.
 * @return array
 *   Contains all files that were found.
 *
 */
function oa_export_export_media_files($field_name, $entity) {

  $files = array();

  $info = field_info_field($field_name);
  // Lets us know if this is a file field.
  if (!empty($info['indexes']) && array_key_exists('fid', $info['indexes'])) {
    if (!empty($entity->{$field_name}) && !empty($entity->{$field_name}[LANGUAGE_NONE])) {

      foreach ($entity->{$field_name}[LANGUAGE_NONE] as $delta => $file) {

        if (!empty($file)) {
          // A file can be an array or object.
          $file = is_object($file) ? (array) $file : $file;

          // Add these files to an array.
          $files[$file['fid']] = $file;

          // This is the actual path to the file.
          $file_source = drupal_realpath($file['uri']);

          // We only copy the file if it doesn't exist.
          if (!file_exists($_SESSION['oa_export']['files_directory'] . '/' . $file['filename'])) {
            // Copy the file to our export directory.
            file_unmanaged_copy($file_source, $_SESSION['oa_export']['files_directory']);
          }
        }
      }
    }
  }

  return $files;
}

/**
 * This will find all media files that need to be exported for a 'node' entity.
 *
 * @param $entity
 * @param $context
 * @param array $files
 * @return array
 */
function oa_export_get_media_files($entity, &$context, $files = array()) {

  // Get all fields for this entity.
  $fields = field_info_instances('node', $entity->type);

  foreach ($fields as $field_name => $field) {
    $files_found = oa_export_export_media_files($field_name, $entity);
    $files += $files_found;
  }
  return $files;
}

/**
 * This will find all media files that need to be exported for a
 * 'paragraphs_item' entity.
 *
 * @param array $items
 *   The paragraph item ids.
 * @param array $context
 * @param array $files
 * @return array $files
 *   Files found that have been added via a paragraph item.
 */
function oa_export_get_paragraph_media_files($items, &$context, $files = array()) {

  foreach ($items as $delta => $item) {
    // Try loading a paragraph item entity. We use this because it takes
    // revisions into account.
    if ($paragraph = paragraphs_field_get_entity($item)) {
      // Add the paragraph to context so we can export it as well.
      $context['paragraphs'][] = $paragraph;
      // Get all fields for this entity.
      $fields = field_info_instances('paragraphs_item', $paragraph->bundle);
      foreach ($fields as $field_name => $field) {
        $files_found = oa_export_export_media_files($field_name, $paragraph);
        $files += $files_found;
      }
    }
  }

  return $files;
}

/**
 * Imports the file and saves an entry into the database.
 *
 * @param array $files
 *   An array of files keyed by the file id of the original file before import.
 *
 * @return array
 *   An array of newly saved files with new file ids keyed by the old file id.
 */
function oa_export_import_media_files($files) {

  $new_files = array();

  // The files that were exported are located here.
  $file_location = $_SESSION['oa_export']['extract_location'] . '/files';

  foreach ($files as $fid => $file) {
    if (!empty($file)) {

      // A file can be an array or object.
      $file = is_object($file) ? (array) $file : $file;

      // Location where the file will be saved.
      $destination = drupal_dirname($file['uri']);

      // The name of the file from the export.
      $filename = drupal_basename($file['uri']);

      // Find the actual file we need to import.
      $file_contents = file_get_contents(realpath($file_location.DIRECTORY_SEPARATOR.$filename));

      // Save the file by the same name to its new location and record it in the
      // database.
      if ($new_file = file_save_data($file_contents, $destination . $filename, FILE_EXISTS_REPLACE)) {
        // Add the file to the array of files.
        $new_files[$fid] = $new_file;
      }
    }
  }

  return $new_files;
}

/**
 * @TODO: This will hopefully become a way to import paragraphs of any entity type.
 *
 * THIS IS WHAT I WAS WORKING ON. NEED TO START HERE....
 *
 * @param $import
 * @param $new_entity
 * @param $old_entity
 * @param $entity_type
 */
function oa_export_import_paragraph_items($import, $new_entity, $old_entity, $entity_type) { // Did have &$import by reference.

  $old_id = $old_entity->nid;

  // Fields on the original node.
  $fields = field_info_instances($entity_type, $old_entity->type);

  // Iterate over the fields to find paragraphs fields.
  foreach ($fields as $field_name => $field) {

    // Right now field_oa_related is the only field we using for paragraphs so
    // we could save some cycles here by hard coding that field but this is
    // better if we ever add any other paragraphs fields.
    $field_info = field_info_field($field['field_name']);

    // This will find fields like field_oa_related that are 'paragraphs' type
    // and make sure the field exists in the node.
    if ($field_info['type'] == 'paragraphs' && isset($new_entity->$field_name)) {
      // We use the original node here because paragraphs_clone_items()
      // unset the paragraph field, in this case (field_oa_related).
      $language = $new_entity->language;

      // Iterate over 'paragraphs' fields that exist in the node we are
      // importing and pass them individually to one of the helper
      // functions that either clones it instantly or stores it
      // for later when the node that is needed has been imported already.
      foreach ($old_entity->{$field_name}[$language] as $delta => $item) {

        // The exported paragraph we will target. The paragraph is stored by
        // [entity_type:item_id:nid].
        $paragraph = $import['paragraphs']['paragraphs_item' . ':' . $item['value'] . ':' . $old_id];

        // Just seeing what we have access to here that may be used to help
        // define a more generic way to handle this.
//        $wrapper = entity_metadata_wrapper('paragraphs_item', NULL, array('bundle' => $paragraph->bundle));
//        $wrapper_entity_info = $wrapper->entityInfo();

        // Information passed to alter functions. May not need all this, just
        // adding for now and will minimize later.
        $data = array(
          'import' => $import,
          'paragraph' => $paragraph,
          'item_id' => $item['value'],
          'entity_type' => 'node',
          'field_name' => $field_name,
          'language' => $language,
        );

        // Allows custom import functions keyed on paragraph bundle to update
        // the node that is being imported.
        drupal_alter('oa_export_import_' . $paragraph['bundle'], $new_entity, $data);

        // Do we need to write a query here that will check to see if the master content
        // that the paragraph relates to has been imported yet. Not sure how we
        // will know what the new nid is of the content we would need to search
        // for is though?? Should be able to just do a node_load($new_nid) to check.

      }
    }
  }
}

/**
 * Imports the file and saves an entry into the database. Pass in an array of
 * files you need to import and this will find them by file name and import
 * them.
 *
 * @param array $files
 *   An array of files that need to be imported/saved keyed by the file id of
 *   the original file before import.
 * @return array
 *   An array of newly saved files with new file ids keyed by the old file id.
 */
function oa_export_import_save_media_files($files) {

  $new_files = array();

  // The files that were exported are located here.
  $file_location = $_SESSION['oa_export']['extract_location'] . '/files';

  foreach ($files as $fid => $file) {
    // Files can be objects or arrays...
    $file = (is_object($file)) ? (array) $file : $file;
    if (!empty($file)) {

      // Location where the file will be saved.
      $destination = drupal_dirname($file['uri']);

      // The name of the file we want to save.
      $filename = drupal_basename($file['uri']);

      $actual_path = realpath($destination . $filename);

      if (!file_exists(realpath($destination . $filename))) {
        // Find the actual file we need to import.
        $file_contents = file_get_contents(realpath($file_location.DIRECTORY_SEPARATOR.$filename));

        // Save the file by the same name to its new location and record it in the
        // database.
        if ($new_file = file_save_data($file_contents, $destination . $filename, FILE_EXISTS_REPLACE)) {
          // Add the file to the array of files.
          $new_files[$fid] = $new_file;
        }
      }
//      // Find the actual file we need to import.
//      $file_contents = file_get_contents(realpath($file_location.DIRECTORY_SEPARATOR.$filename));
//
//      // Save the file by the same name to its new location and record it in the
//      // database.
//      if ($new_file = file_save_data($file_contents, $destination . $filename, FILE_EXISTS_REPLACE)) {
//        // Add the file to the array of files.
//        $new_files[$fid] = $new_file;
//      }
    }
  }
  return $new_files;
}

function oa_export_import_comment_paragraph_items($comments, $new_nid, $paragraphs, $import) {
  foreach ($comments as $old_cid => $new_cid) {

    // Load the comment.
    $comment = comment_load($new_cid);

    // See if there is any related content from paragraphs.
    if (!empty($comment->field_oa_related)) {

      // Look for the paragraph item that belongs to this comment.
      foreach ($paragraphs as $cid => $paragraph) {

        // Compare the cid from the paragraph with the old_cid from the comment.
        // If these match then we have found the correct comment.
        if ($cid == $old_cid) {

          $target_paragraph = $paragraphs[$cid];

          // We found the paragraph so lets create it.
          foreach ($target_paragraph as $item) {

            $array = array();

            // This will return a new paragraph item entity. (This may need to replace $comment->field_oa_related['und'][0]['entity'] = $new_paragraph_item
            $new_paragraph = paragraphs_field_get_entity($array, $item->bundle, $item->field_name);

            // Set the paragraph item for the comment.
            $comment->field_oa_related[LANGUAGE_NONE] = array(
              0 => array(
                'entity' => $new_paragraph,
              ),
            );

            // Re-save the comment with the paragraph entity.
            comment_save($comment);

            // What I think... The comment field_oa_related item_id refers to the paragraph item_id
            // The actually paragraph item ($new_paragraph) field_oa_related content target_id
            // refers to the nid of the content that it's referencing.

//            $comment->field_oa_related[LANGUAGE_NONE][0]['value'] = $new_paragraph->item_id;
//            $comment->field_oa_related[LANGUAGE_NONE][0]['revision_id'] = $new_paragraph->revision_id;

            switch ($item->bundle) {

              case 'paragraph_content':

                $load_item = array(
                  'value' => $new_paragraph->item_id,
                );

                // Now load the new paragraph item.
                $load_paragraph = paragraphs_field_get_entity($load_item);

                // This is the original nid of the related content.
                $related_content = $item->field_oa_related_content[LANGUAGE_NONE][0]['target_id'];

                foreach ($import['sections'] as $section_id => $section_data) {

                  if (!empty($section_data['section_content'][$related_content])) {
                    foreach ($section_data['section_content'] as $old_section_target_nid => $section_target_content) {
                      if ($related_content == $old_section_target_nid) {

                      }
                    }
                  }
                }
                break;
            }
          }
        }
      }
    }
  }
}

/**
 * Finds all comments for a given node.
 *
 * @param int $nid
 *   The node id you want to find comments for.
 * @return mixed
 *   The comment ids.
 */
function oa_export_find_comments($nid) {
  $comments = db_select('comment', 'c')
    ->fields('c', array('cid', 'nid'))
    ->condition('nid', $nid, '=')
    ->execute()
    ->fetchAllKeyed();

  return array_keys($comments);
}

/**
 * Prepares a comment for import.
 *
 * @param object $comment
 *   The comment being prepared.
 * @param $nid
 *   The nid of the node the comment belongs to.
 */
function oa_export_import_prepare_comment(&$comment, $nid) {
  // We want to create a new comment.
  $comment->cid = NULL;
  $comment->created = NULL;
  $comment->changed = NULL;
  // We want to associate the comment with the content it belongs to.
  $comment->nid = $nid;
}

/**
 * Import comments for a node.
 *
 * @param $comments
 * @param $new_nid
 * @throws Exception
 */
function oa_export_import_save_comments($comments, $new_nid) {

  // Storage for comments that have parents.
  $comment_children = array();

  // Storage for all comments we find.
  $all_comments = array();

  foreach ($comments as $target => $comment) { // We add a layer here so we have a place to store not only the comment but paragraphs.
    $entity = $comment['entity'];

    /* @todo: Something is being passed by reference. The original comment updates when the new one is prepared. */

    // Import all comments that don't have a parent comment.
    if (empty($entity->pid)) {

      // Comment cid before saving.
      $before = $entity->cid;

      // Prepare the comment for import.
      oa_export_import_prepare_comment($entity, $new_nid);
      comment_save($entity);

      // Comment cid after saving a new comment.
      $after = $entity->cid;

      // Add cids to an array for later.
      $all_comments[$before] = $after;
      // We want to create a new comment.
//              oa_export_import_comment($comment, $old_section_content, $new_section_content->nid, $import_sections);
    }
    else {
      // Build an array of comments that need a parent created first.
      $comment_children[] = $entity;
    }
  }
  /** @todo Figure out a better way to do this. */
  // This seems redundant but we must get all parent comments
  // imported first and then import child comments so they can be
  // associated with their parent.
  foreach ($comment_children as $comment) {
    // If there is a pid then this is a child comment.
    if (!empty($comment->pid)) {

      // Comment cid before saving.
      $before = $comment->cid;

      // Prepare the comment for import.
      oa_export_import_prepare_comment($comment, $new_nid);
      comment_save($comment);

      // Comment cid after saving a new comment.
      $after = $comment->cid;

      // Add cids to an array for later.
      $all_comments[$before] = $after;
      // We want to create a new comment.
//              oa_export_import_comment($comment, $old_section_content, $new_section_content->nid, $import_sections, $comment->pid);
    }
  }

  return $all_comments;
}
