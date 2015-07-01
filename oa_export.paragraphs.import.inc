<?php

function oa_export_oa_export_import_paragraph_media_alter($new_entity, $data) {

  $field_name = $data['field_name'];
  $paragraph = $data['paragraph'];

  // Get field instances for paragraph items of this paragraph bundle.
  $fields = field_info_instances('paragraphs_item', $paragraph['bundle']);

  // Create the new paragraph entity item.
  $new_item = entity_create('paragraphs_item', array(
    'bundle' => $paragraph['bundle'],
    'field_name' => $field_name,
  ));

  // Set the host entity to the node entity that was already created.
  $new_item->setHostEntity($data['entity_type'], $new_entity, $data['language']);

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
}

function oa_export_oa_export_import_paragraph_text_alter($new_entity, $data) {

  $field_name = $data['field_name'];
  $paragraph = $data['paragraph'];

  // Get field instances for paragraph items of this paragraph bundle.
  $fields = field_info_instances('paragraphs_item', $paragraph['bundle']);

  // Create the new paragraph entity item.
  $new_item = entity_create('paragraphs_item', array(
    'bundle' => $paragraph['bundle'],
    'field_name' => $field_name,
  ));

  // Set the host entity to the node entity that was already created.
  $new_item->setHostEntity($data['entity_type'], $new_entity, $data['language']);

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
      }
    }
  }
}

function oa_export_oa_export_import_paragraph_content_alter($new_entity, $data) { // Did have &$data by reference.

  $paragraph = $data['paragraph'];
  $import = &$data['import'];

  // Get field instances for paragraph items of this paragraph bundle.
  $fields = field_info_instances('paragraphs_item', $data['paragraph']['bundle']);

  foreach ($fields as $new_field_name => $new_field_instance) {
    if (!empty($paragraph[$new_field_name])) {
      foreach ($paragraph[$new_field_name][LANGUAGE_NONE] as $delta => &$info) {
        // This is the target content we need to import.
        if (isset($info['target_id'])) {

          // Need to check for random content that has been related that is not
          // a Space or Section.
          if (isset($paragraph['content'])) {

            // Iterate over the content and import it.
            foreach ($paragraph['content'] as $key => &$elements) {

              // Make sure we haven't imported this one yet.
              if (!isset($elements['entity']->oa_imported)) {

                list($type,$old_nid,$item_id) = explode(':', $key);

                // This is the piece of content for the paragraph. We check for
                // int here because on export this can also be set to a string,
                // oa_space, oa_section, oa_group, and if so then the value will
                // be the 'nid' of the Space, Section, Group that needs to be
                // imported first before the paragraph is created.
                // This way we can store something about the paragraph and the
                // node (we have the id so we can check for it in the batch.

                /* @todo: hmmm...not sure about this. */
                if (!is_int($elements) && $old_nid == $info['target_id']) {

                  // Prepare the node for import.
                  $cloned_node = oa_clone_prepare($elements['entity'], NULL, NULL, FALSE, $elements);

                  // Need to unset this so oa_clone_node_insert() falls through.
                  unset($cloned_node->clone_from_original_nid);
                  // Save the new entity.
                  node_save($cloned_node);

                  // Import any data related to this entity.
                  oa_export_import_entity_data($elements, $cloned_node, $elements['entity']);

                  // Lets us know we already imported the content for the paragraph.
                  $elements['entity']->oa_imported = TRUE;
                }
              }
            }
          }
          // Need to import this content now so we can reference the new
          // content??
          foreach ($data['import']['content'] as $target => $content) {

            // The node is keyed by the type:nid.
            list($name, $id) = explode(':', $target);

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
                  $import['storage'][$target]['content'] = $content[$name];
                  $import['storage'][$target]['entity'] = $new_entity->nid;
                  $import['storage'][$target]['paragraph'][$data['item_id']] = $data['paragraph'];

                  // We don't want to clone this later since we are
                  // cloning it now.
//                        $data['section']->oa_clone_skip = TRUE;
                  // Clone the section and pass in the export just for the section.
//                        $new_section = oa_clone($data['section'], NULL, NULL, TRUE, $content[$name . ':' . $id]);
                  break;
              }
            }
          }
        }
      }
    }
  }
}

function oa_export_oa_export_import_paragraph_snippet_alter($new_entity, $data) {

  // Get field instances for paragraph items of this paragraph bundle.
  $fields = field_info_instances('paragraphs_item', $data['paragraph']['bundle']);

}
