<?php

require_once 'oa_export.utils.inc';

/**
 * Prepare the data in the compressed file for import.
 *
 * @param string $archive
 *   The path to the archive that is being imported.
 */
function oa_export_batch_import($files, $extract_location) {

  $batch = array(
    'title' => t('Importing Blueprint'),
    'init_message' => t('Preparing to import the blueprint'),
    'finished' => 'oa_export_import_batch_finished',
    'file' => drupal_get_path('module', 'oa_export') . '/oa_export.batch.import.inc',
  );

  // Decode the json files from the export.
  $import = oa_export_decode_archive_data($files, $extract_location);

  // Import the space first.
  oa_export_batch_import_operations($batch, $import['space']);

  // Now we can import subspaces.
  foreach ($import as $key => $values) {

    list($type) = explode(':', $key);

    if ($type == 'subspace') {
      oa_export_batch_import_operations($batch, $values);
    }
  }

  // Import the blueprint last.
  $batch['operations'][] = array('oa_export_batch_import_blueprint', array($import['blueprint']));

  // Define the batch.
  batch_set($batch);
}

/**
 * Decodes the json files from the export.
 *
 * @param $files
 * @param $extract_location
 * @return array
 *   The space and blueprint being imported.
 */
function oa_export_decode_archive_data($files, $extract_location) {

  $import = array();

  foreach ($files as $path) {
    $filename = basename($path);
    $realpath = realpath($extract_location . DIRECTORY_SEPARATOR . $filename);

    list($name) = explode('.', $filename);

    if (isset($name)) {
      list($type,$id) = explode(':', $name);
    }

    switch ($type) {
      case 'blueprint':
        // There will be only one blueprint.
        $blueprint = oa_export_import_define_data($realpath);
        break;

      // There can be only one 'space' for the blueprint, but many
      // subspaces.
      case 'space':
        $import[$type] = oa_export_import_define_data($realpath);
        break;

      case 'subspace':
        $import[$type . ':' . $id] = oa_export_import_define_data($realpath);
        break;

      default:
        break;
    }
  }

  // Add the blueprint to the import.
  $import['blueprint'] = $blueprint;

  return $import;
}

/**
 * Gets our json file and decodes it.
 *
 * @param $path
 *   The real or absolute path to our json file.
 * @return array|object
 *   The object or array created.
 */
function oa_export_import_define_data($path) {
  if ($json = file_get_contents($path)) {
    return oa_export_json_import($json);
  }
  else {
    drupal_set_message(t('Error getting the contents of %path: %message', array('%path' => $path, '%message' => $e->getMessage())));
  }
}

/**
 * @todo: Left off triggering operations for space, section, section content, etc for import.
 * @todo: Dumped the database with all the crazy content. Added a single space with content just to see if we could get it to import correctly.
 */
function oa_export_batch_import_operations(&$batch, $import) {

  // Imports the space.
  $batch['operations'][] = array('oa_export_batch_import_space', array($import));
//  $batch['operations'][] = array('oa_export_batch_import_space_test', array($import));

  if (isset($import['content'])) {
    foreach ($import['content'] as $target => $data) {

      list($name,$original_id) = explode(':', $target);

      if ($name == 'section' && !empty($data['entity'])) {
        $batch['operations'][] = array('oa_export_batch_import_section', array($import, $data['entity'], $original_id));

        if (!empty($data['content'])) {
          foreach ($data['content'] as $key => $value) {

            list($type,$id) = explode(':', $key);

            if ($type == 'section_content' && !empty($value['entity'])) {
              $batch['operations'][] = array('oa_export_batch_import_section_content', array($import, $value['entity'], $id));
            }
          }
        }
      }
      // Subspaces
      if (module_exists('oa_subspaces')) {

        if ($name == 'subspace' && !empty($data['entity'])) {
          oa_export_batch_operations($batch, $data);
        }
      }
    }
  }
}

/**
 * Implements hook_clone_node_alter().
 *
 * This is called before @see paragraphs_clone_node_alter(). We need to run this
 * after so we can update any files that have been associated with the node.
 */
function oa_export_clone_node_alter(&$node, &$context) {
  $export = (isset($context['export'])) ? $context['export'] : NULL;

  // We only need to run this if we are importing.
  if (isset($export) && is_array($export)) {

    $original_node = $context['original_node'];

    // Fields on the original node.
    $fields = field_info_instances('node', $original_node->type);

    // Make sure files exist.
    /* @todo: Make sure we export files for random pieces of content. */
    $files_exist = isset($export['files']) && !empty($export['files']);

    // Iterate over the fields to find paragraphs fields.
    foreach ($fields as $field_name => $field) {

      // Right now field_oa_related is the only field we using for paragraphs so
      // we could save some cycles here by hard coding that field but this is
      // better if we ever add any other paragraphs fields.
      $info = field_info_field($field['field_name']);

      // We will go ahead and import any files that exist for this specific
      // node.
      if (!empty($info['indexes']) && array_key_exists('fid', $info['indexes'])) {
        if (!empty($original_node->{$field_name}) && !empty($original_node->{$field_name}[LANGUAGE_NONE])) {

          foreach ($original_node->{$field_name}[LANGUAGE_NONE] as $delta => $original_file) {

            if ($files_exist && isset($export['files'][$original_file['fid']])) {
              // This saves the files and returns the newly saved files for the
              // node keyed by the original fid.
              $new_files = oa_export_import_save_media_files(array($original_file['fid'] => $export['files'][$original_file['fid']]));

              // We need to update the file ids for the space files.
              foreach ($new_files as $original_fid => $new_file) {
                if ($original_file['fid'] == $original_fid) {

                  // The file could be an object.
                  $new_file = (is_object($new_file)) ? (array) $new_file : $new_file;

                  // Merge the new and old files. There are things like width
                  // and height we need.
                  $new_space_file = array_merge($original_file, $new_file);

                  // Set the new field in the node we are cloning.
                  $node->{$field_name}[LANGUAGE_NONE][$delta] = $new_space_file;
                }
              }
            }
          }
        }
      }
      // This will find fields like field_oa_related that are 'paragraphs' type
      // and make sure it exists in the node.
//      if ($info['type'] == 'paragraphs' && isset($node->$field_name)) {
//        // We use the original node here because paragraphs_clone_items()
//        // unsets the field we need (field_oa_related).
//        $language = $original_node->language;
//        foreach ($original_node->{$field_name}[$language] as $delta => $item) {
//          oa_export_clone_paragraphs_items('node', $node, $export, $field_name, $language);
//        }
//      }
    }
  }
}

/**
 * Will either clone or store the paragraph item. We could have a paragraph
 * item that relates to content that hasn't been created yet. In this case, the
 * data is stored in our import and will be cloned when the content is saved.
 *
 * @param array $import
 *   The full import that is being passed around.
 * @param array $context
 *   We use this to store data that may need to be imported later.
 * @param array $paragraph
 *   The paragraph to clone.
 * @param string $entity_type
 *   The entity type, e.g., 'node'.
 * @param object $new_entity
 *   The entity that this paragraph is being created for.
 * @param string $field_name
 *   The field that is a of type 'paragraphs', e.g., field_oa_related.
 * @param string $language
 *   The language code. Defaults to 'und'.
 *
 * @TODO: Fix this function!!!!!!!!!!!!!!!!!
 */
function oa_export_clone_import_paragraph_item($import, &$context, $paragraph, $entity_type, &$new_entity, $field_name, $language = LANGUAGE_NONE) {

  // Get field instances for paragraph items of this paragraph bundle.
  $fields = field_info_instances('paragraphs_item', $paragraph['bundle']);

    // We need to switch on the bundle.
    switch ($paragraph['bundle']) {

      // For media we can go ahead and import the file and set the field.
      case 'paragraph_media':

        // Create the new paragraph entity item.
        $new_item = entity_create('paragraphs_item', array(
          'bundle' => $paragraph['bundle'],
          'field_name' => $field_name
        ));

        // Set the host entity to the node entity that was already created.
        $new_item->setHostEntity($entity_type, $new_entity, $language);

        $final_files = array();

        foreach ($fields as $new_field_name => $new_field_instance) {
          if (!empty($paragraph[$new_field_name])) {
            // We need to update the fid if this is a file field.
            foreach ($paragraph[$new_field_name][LANGUAGE_NONE] as $delta => &$file) {

              // Set the field.
              $new_item->{$new_field_name} = $paragraph[$new_field_name];

              // Make sure this is a file.
              if (isset($file['fid'])) {

                // Find the file in our file exports and save the file.
                $new_file = array_shift(oa_export_import_save_media_files(array($file['fid'] => $file)));

                // Files can be objects and we need an array to merge.
                $new_file = is_object($new_file) ? (array) $new_file : $new_file;

                // Merge the files so we get all settings from the original file.
                $merged_file = array_merge($file, $new_file);

                $final_files[$delta] = $merged_file;

                if (!empty($final_files)) {
                  $new_item->{$new_field_name}[LANGUAGE_NONE] = $final_files;
                }
              }
            }
            /** @todo: Need to figure out how to handle this recursive function if need be. */

            $field_info = field_info_field($new_field_name);
            if ($field_info['type'] == 'paragraphs') {

              // This means there is a paragraph within a paragraph?
              $stop = TRUE;

              // As this is now, passing $import back in will cause this to be a never ending loop!!!
//                oa_export_clone_paragraphs_items($import, $context, $paragraphs, 'paragraphs_item', $new_item, $new_field_name, $language);
            }
          }
        }
        break;

      case 'paragraph_text':
        // Create the new paragraph entity item.
        $new_item = entity_create('paragraphs_item', array(
          'bundle' => $paragraph['bundle'],
          'field_name' => $field_name
        ));

        // Set the host entity to the node entity that was already created.
        $new_item->setHostEntity($entity_type, $new_entity, $language);

        foreach ($fields as $new_field_name => $new_field_instance) {
          if (!empty($paragraph[$new_field_name])) {
            foreach ($paragraph[$new_field_name][LANGUAGE_NONE] as $delta => &$info) {

              // Set the field.
              $new_item->{$new_field_name} = $paragraph[$new_field_name];

            }

            /** @todo: Need to figure out how to handle this recursive function if need be. */

            $field_info = field_info_field($new_field_name);
            if ($field_info['type'] == 'paragraphs') {

              // This means there is a paragraph within a paragraph?
              $stop = TRUE;

              // As this is now, passing $import back in will cause this to be a never ending loop!!!
//                oa_export_clone_paragraphs_items($import, $context, $paragraphs, 'paragraphs_item', $new_item, $new_field_name, $language);
            }
          }
        }
        break;

      default:
        break;
    }
}

function oa_export_store_import_paragraph_item($import, &$context, $paragraph, $entity_type, &$new_entity, $field_name, $language = LANGUAGE_NONE) {

    // Get field instances for paragraph items of this paragraph bundle.
    $fields = field_info_instances('paragraphs_item', $paragraph['bundle']);

    // Check if any of the fields in the newly cloned item is a paragraph.
//    foreach ($fields as $new_field_name => $new_field_instance) {
//      if (!empty($paragraph[$new_field_name])) {

    // We need to switch on the bundle.
    switch ($paragraph['bundle']) {

      // For content we will need to store this for later. We will handle
      // this when the actual node the content belongs to has been saved.
      case 'paragraph_content':
        // This is where the exported content lives.
        $content = $import['content'];

        foreach ($fields as $new_field_name => $new_field_instance) {
          if (!empty($paragraph[$new_field_name])) {
            foreach ($paragraph[$new_field_name][LANGUAGE_NONE] as $delta => &$info) {
              // This is the target content we need to import.
              if (isset($info['target_id'])) {
                // Need to import this content now so we can reference the new
                // content??
                foreach ($content as $target => $data) {

                  // The node is keyed by the type:nid.
                  list($name,$id) = explode(':', $target);

                  // We only want to continue if the id matches the target id
                  // of the related content.
                  if ($id == $info['target_id']) {

                    switch ($name) {
                      case 'section':
                        // Can we check to see if the section exists yet? Then
                        // We could actually just use this same function to
                        // handle paragraph content.


                        // Store this section so when we import it later we can
                        // import this paragraph at that time. We will store
                        // the id of the entity so we can load it again later if
                        // we need to or just associate the section as related
                        // content on this entity.
                        $context['results']['stored_sections'][$name . ':' . $id] = $data['section'];
                        $context['results']['stored_sections']['entity'] = $new_entity->nid;
                        $context['results']['stored_sections']['paragraph'][$paragraph->item_id] = $paragraph;

                        // We don't want to clone this later since we are
                        // cloning it now.
//                        $data['section']->oa_clone_skip = TRUE;
                        // Clone the section and pass in the export just for the section.
//                        $new_section = oa_clone($data['section'], NULL, NULL, TRUE, $content[$name . ':' . $id]);
                        break;

                      case 'section_content':

                        $context['results']['stored_section_content'][$name . ':' . $id] = $data['section_content'];
                        $context['results']['stored_section_content']['entity'] = $new_entity->nid;
                        $context['results']['stored_section_content']['paragraph'][$paragraph->item_id] = $paragraph;

                        // We don't want to clone this later since we are
                        // cloning it now.
//                        $data['section_content']->oa_clone_skip = TRUE;
//                        $new_section_content = oa_clone($data['section_content'], NULL, NULL, TRUE, $content[$name . ':' . $id]);
                        break;
                    }
                  }
                }
              }
            }
          }
        }
        break;

      case 'paragraph_snippet':
        break;

      default:
        break;
    }
//      }
//    }
}

/**
 * This simply stores the nid of the node that was just created so we can use it
 * for other nodes that are being cloned via oa_clone_prepare().
 *
 * @param $node
 */
function oa_export_node_insert($node) {
  // Set some session variables so we have the nids of nodes so we can reference
  // the correct nids when cloning other content such as related content.
  if (in_array($node->type, array('oa_section', 'oa_space', 'oa_group'))) {
    switch ($node->type) {
      case 'oa_space':
        $_SESSION['oa_export']['space'] = $node->nid;
        break;
      case 'oa_section':
        $_SESSION['oa_export']['section'] = $node->nid;
        break;
      case 'oa_group':
        $_SESSION['oa_export']['group'] = $node->nid;
        break;
      default:
        break;
    }
  }
}

/**
 * Helper function that updates media files that are attached to
 * paragraph items.
 *
 * @param $entity_type
 * @param $entity
 * @param $field_name
 * @param string $language
 */
function oa_export_import_update_paragraphs($entity_type, &$entity, $original_entity, $field_name, $language = LANGUAGE_NONE) {


  foreach ($entity->field_oa_related[LANGUAGE_NONE] as $item) {
    // Try loading a paragraph item entity. We use this because it takes
    // revisions into account.
    if ($paragraph = paragraphs_field_get_entity($item)) {
      $file_fields = array_merge($file_fields, oa_export_import_find_media_file_fields('paragraphs_item', $paragraph, $context));
      return $file_fields;
    }
  }


  $entity_wrapper = entity_metadata_wrapper($entity_type, $entity);
  $old_items = $entity_wrapper->{$field_name}->value();
  if (!is_array($old_items)) {
    $old_items = array($old_items);
  }
  foreach ($old_items as $delta => $old_item) {
    if (isset($old_item->field_oa_media) && !empty($old_item->field_oa_media)) {
      // Save the files to the file system.
      $original_file = $old_item->field_oa_media[$language][0];

      $saved_file = oa_export_import_media_files(array($original_file['fid'] => $original_file));

      foreach ($saved_file as $original_fid => $new_file) {
        // Files are sometimes objects and we need an array.
        $new_file = (is_object($new_file)) ? (array) $new_file : $new_file;
        // Merge the new and old files. There are things like width and height
        // we need.
        $merged_file = array_merge($original_file, $new_file);

        // Replace the file in field_oa_media.
        $entity_wrapper->{$field_name}->value()[$delta] = $merged_file;
      }
    }
  }
}

function oa_export_import_comment($comment, $old_section_content, $old_section_content_nid, $import_sections, $pid = NULL) {

  $paragraph = TRUE;

  // We need to get the content nid if we are going to update a comment.
  if (isset($comment->oa_export_target)) {
    $content_reference_nid = $comment->oa_export_target['new_related_content_id'];
  } // Add the rest to an else.
  // If we find a comment then we know we have already imported this comment and
  // we need to update it.
  if ($comment_clone = comment_load($comment->cid)) {
    $paragraph = FALSE;
  }
  // If we don't find a comment then this is a new comment that needs import.
  else {
    $comment_clone = clone $comment;
  }

  if ($paragraph) {
    // See if there is any related content from paragraphs.
    if (!empty($comment->field_oa_related)) {
      // Get the id of the paragraph item that belongs to this comment.
      $item = $comment->field_oa_related[LANGUAGE_NONE][0];
      // Get the paragraph entity that belongs to the id we stored in our export.
      $related_paragraph_item = $old_section_content['paragraphs'][$comment->cid][$item['value']];

      /** todo: This is a hack. Need to figure out why when the json is decoded it leaves " = null " with an empty key*/
      // This won't happen if we just export what we need from the paragraph
      // entity and not the entire entity.

      $z = (array) $related_paragraph_item;
      $related_paragraph_item = (object) $z;
      // Reset the item array.
      $item = array();
      // This should create a new paragraph item entity. (This may need to replace $comment->field_oa_related['und'][0]['entity'] = $new_paragraph_item
      $new_paragraph_item = paragraphs_field_get_entity($item, $related_paragraph_item->bundle, $related_paragraph_item->field_name);
      // Set field_oa_related
      unset($comment_clone->field_oa_related['und']);
      $comment_clone->field_oa_related['und'][0]['entity'] = $new_paragraph_item;

      // Prepare the comment for import.
      oa_export_import_prepare_comment($comment_clone, $old_section_content_nid);
      comment_save($comment_clone);

      // Now we need to merge the other fields that existed in the $related_paragraph_item.
      $merged_paragraph_item = (object) array_merge((array) $new_paragraph_item, (array) $related_paragraph_item);

      // We need to probably switch on the $merged_paragraph_item for the bundle so we can look for the appropriate fields.
      switch ($merged_paragraph_item->bundle) {
        case 'paragraph_content':
          $old_related_content_id = $merged_paragraph_item->field_oa_related_content[LANGUAGE_NONE][0]['target_id']; // This is the nid of the content that was related to this paragraph item. We will need this so we can find the correct piece of content and make sure it is imported.

          // We need to iterate over the sections to find the content that relates
          // and import it now so we can associate the related field with it.
          foreach ($import_sections as $section_id => $section_target) {
            // We found the content we need.
            if (!empty($section_target['section_content'][$old_related_content_id])) {
              foreach ($section_target['section_content'] as $old_section_target_nid => $section_target_content) {
                if ($old_related_content_id == $old_section_target_nid) {

                  // Setup our import to include the sections and section content.
                  $sub_import['sections'][$section_id] = array(
                    'section' => $section_target['section'],
                    'section_content' => $section_target['section_content']
                  );

                  // Remove this section from the main $import_sections array so
                  // it doesn't try to get imported later.
                  unset($import_sections[$section_id]);

                  // Add a field to the comment so we know what we are looking for.
                  $comment_clone->oa_export_target = array(
                    'old_related_content_id' => $old_related_content_id,

                  );
                  // We need to import this section and section content.
                  oa_export_import_space($sub_import, $comment_clone);
                  $paragraph = FALSE;
//                  unset($comment_clone->oa_export_target, $merged_paragraph_item, $new_paragraph_item, $old_related_content_id);
                }
              }
            }
          }
          break;
        case 'paragraph_media':
          // Check the media field for files.
          if (!empty($merged_paragraph_item->field_oa_media)) {
            // We use this to find files related to the content.
            $wrapper = entity_metadata_wrapper('node', $merged_paragraph_item);
            oa_export_import_field_oa_media($wrapper);
          }
          break;
        case 'paragraph_snippet':
          break;
        case 'paragraph_text':
          break;
        default:
          break;
      }
    }
  }
  // This means we had to import a piece of content that is related.
  if (isset($content_reference_nid)) {
    // Update the comment so it contains the correct reference to content.
    $comment_clone->field_oa_related[LANGUAGE_NONE][0]['value'] = $content_reference_nid;
    $comment_clone->field_oa_related[LANGUAGE_NONE][0]['revision_id'] = $content_reference_nid;
  }
  // Prepare the comment for import.
  oa_export_import_prepare_comment($comment_clone, $old_section_content_nid);
  comment_save($comment_clone);

  if (isset($content_reference_nid)) {
    unset($content_reference_nid);
  }
}

function oa_export_import_field_oa_media(&$wrapper) {
  // Iterate over the wrapper we created to find media files.
  foreach ($wrapper->field_oa_media->getIterator() as $delta => $media_wrapper) {
    // This may now be accessed as a media wrapper.
    $original_file = $media_wrapper->value();
    // Get the 'real' path for the files uri.
    $directory_path = drupal_realpath(drupal_dirname($original_file['uri']));
    // Make sure the path exists for the file.
    if (file_prepare_directory($directory_path, FILE_CREATE_DIRECTORY)) {
      // We only need one file so we only copy it once.
      /** @todo: figure out why it tries to copy a file more than once */
      if (!file_exists($directory_path . '/' . $original_file['filename'])) {
        // Copy the file to it's new location.
        $tmp_file_location = $_SESSION['oa_export']['directory'] . '/files' . $original_file['filename'];
        $copy = file_unmanaged_copy($tmp_file_location, $directory_path, FILE_EXISTS_REPLACE);
      }
    }
    // Unset the fid so it generates a new one when saved.
    $original_file['fid'] = NULL;
    // Make sure we don't already have a file by the same name.
    $result = db_select('file_managed', 'fm')
      ->fields('fm', array('fid'))
      ->condition('uri', $original_file['uri'])
      ->execute();
    foreach ($result as $record) {
      $new_file = file_load($record->fid);
    }
    if (!isset($new_file)) {
      // Create a new database entry for the file. It requires an object
      // so we must cast it as such.
      $new_file = file_save((object) $original_file);
    }
    // Convert the file back to an array.
    $new_file = (array) $new_file;
    // Attach the file to the content node.
    $wrapper->field_oa_media[$delta] = $new_file;
    // We need to unset the file.
    unset($new_file);
    // Save the content with the file.
    $wrapper->save();
  }
}


/**
 * Imports a blueprint via a json file.
 *
 * @param $batch
 * @param $blueprint
 * @param $space
 */
function _oa_export_batch_import_blueprint(&$batch, $blueprint, $space) {

  // Export a json file that contains the blueprint.
  $export = oa_export_create_json_export('blueprint', $blueprint, $_SESSION['oa_export']['directory']);

  // @todo: Add this in the finish function.
  $_SESSION['oa_export']['blueprint'] = $blueprint;

  // Add some operations to our batch process.
  oa_export_batch_import_operations($batch, $space);
}

/**
 * Not being used now. A first run at trying to import data.
 *
 * @param array $import
 * @throws Exception
 */
function oa_export_import_data(array $import) {
  // Store the new space nid because we will need it so we can relate future
  // content to it. You can only import one space at a time so we know this
  // won't change.
  static $new_space_nid;

  // We need to import the space first.
  foreach($import['space'] as $space) {
    $space_node = oa_clone($space, NULL, NULL, FALSE, TRUE);
    $space_nid = $space_node->nid;
  }
  // Now we can import the blueprint.
  foreach ($import['blueprint'] as $blueprint) {
    // Unset the tid of this blueprint so it will generate a new one when
    // it is imported.
    $blueprint->tid = NULL;
    // Make sure we have the correct vocabulary vid.
    $vocab = taxonomy_vocabulary_machine_name_load('space_type');
    // Associate this taxonomy term with the correct vocabulary.
    $blueprint->vid = $vocab->vid;
    // Set the space reference to the nid of the space we just created.
    $blueprint->field_oa_clone_space[LANGUAGE_NONE][0]['target_id'] = $new_space_nid;
    taxonomy_term_save($blueprint);
  }
  // Next we can import any sections this space might contain along with any
  // content for each section.
  foreach ($import['sections'] as $old_section_nid => $section) {
    // Create a new section node associating it with the space.
    $section_node = oa_clone($section, $new_space_nid, NULL, FALSE, TRUE);
    // We know we have a piece of content that belongs to this section.
    if (isset($import['section_content'][$old_section_nid])) {
      $original_content_nid = $import['section_content'][$old_section_nid]->nid;
      // Create the new content node. We need the original node, the space nid,
      // and the section nid that relates to this piece of content.
      $new_section_content = oa_clone($import['section_content'][$old_section_nid], $new_space_nid, $section_node->nid, FALSE, TRUE);
      // Check for comments that are associated with this piece of content.
      if (isset($import['comments'][$original_content_nid])) {
        foreach ($import['comments'][$original_content_nid] as $comment) {
          if (empty($comment->pid)) {
            // We want to create a new comment.
            $comment->cid = NULL;
            // @todo: Do we want to set 'created' and 'changed' to NULL?
            // We want to associate the comment with the content it belongs to.
            $comment->nid = $new_section_content->nid;
            comment_save($comment);
          }
          else {
            continue;
          }
        }
        // @todo: this seems redundant
        foreach ($import['comments'][$original_content_nid] as $comment) {
          if (!empty($comment->pid)) {
            // We want to create a new comment.
            $comment->cid = NULL;
            // We want to associate the comment with the content it belongs to.
            $comment->nid = $new_section_content->nid;
            comment_save($comment);
          }
          else {
            continue;
          }
        }
      }
      // Check to see if a file exists for this content.
      if (isset($import['content_files'][$original_content_nid]) && $file_import = $import['content_files'][$original_content_nid]) {
        $uri = $file_import['default']['uri'];
        $destination = drupal_dirname($uri);
        $directory_path = drupal_realpath(drupal_dirname($uri));
        $file_path = drupal_realpath($uri);
        if (file_prepare_directory($directory_path, FILE_CREATE_DIRECTORY)) {
          $decoded_file = base64_decode($file_import['encoded']);
//          $write = file_put_contents($file_path, $new_file);
//          if ($write) {
            $file = file_save_data($decoded_file, $uri);
            if (is_object($file)) {
              $file = (array)$file;
              // Merge the new file with the original file.
              $new_file = $file_import['default'] + $file;
              // Set the new fid. This doesn't get set.
              $new_file['fid'] = $file['fid'];
              // Attach the file to the content node.
              $new_section_content->field_oa_media[LANGUAGE_NONE][] = $new_file;
              // Resave the content with the file.
              node_save($new_section_content);
            }
//          }
        }
      }

    }
  }
  // Next we will create any subspaces that may exist for this space.
//  foreach ($import['subspaces'] as $subspace) {
//    $subspace_node = oa_clone($subspace, $space_nid, NULL, FALSE, TRUE);
//  }
}
