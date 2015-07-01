<?php

/**
 * Exports files found in paragraph_media items.
 *
 * @param array $paragraph_export
 *   The paragraph item that will be exported.
 * @param array $data
 *   Contains data needed for this function.
 */
function oa_export_oa_export_paragraph_media_alter(&$paragraph_export, $data) {

  $field_name = $data['field_name'];
  $paragraph = $data['paragraph'];

  oa_export_export_media_files($field_name, $paragraph);
}

/**
 * Finds paragraphs and content.
 *
 * @param array $paragraph_export
 *   The paragraph item that will be exported.
 * @param array $data
 *   Contains data needed for this function.
 */
function oa_export_oa_export_paragraph_content_alter(&$paragraph_export, $data) {

  $field_name = $data['field_name'];
  $paragraph = $data['paragraph'];

  foreach ($paragraph->{$field_name}[LANGUAGE_NONE] as $delta => $info) {
    if (isset($info['target_id'])) {
      $id = $info['target_id'];
      // Load the content so we can store it with the paragraph.
      $node = node_load($id);

      // @todo: Need to also check for files on this content and store them below.
      $files = oa_export_get_media_files($node, $data['context']);

      // Need to just store the nid here. More than likely most content stored
      // will be a section or subspace or something being exported later but
      // there is a chance this can be a random piece of content like a
      // panopoly_page. If that's the case we would want to store the whole node.
      // We don't want to store the whole node for something that will be exported
      // later as we will end up with a recursive issue with the json export.

      // Maybe we just check its type and export the full object if it's
      // not one of the 3 primary.
      if (in_array($node->type, array('oa_section', 'oa_space', 'oa_group'))) {
        // We only store the nid since we know the full object will be stored later.
        $paragraph_export['content'][$node->type . ':' . $node->nid . ':' . $paragraph->item_id] = $node->nid;
      }
      else {
        // Store the full object since this content won't be exported later.
        $paragraph_export['content'][$node->type . ':' . $node->nid . ':' . $paragraph->item_id]['entity'] = $node;
        if (!empty($files)) {
          $paragraph_export['content'][$node->type . ':' . $node->nid . ':' . $paragraph->item_id]['files'] = $files;
        }
      }
    }
  }
}

