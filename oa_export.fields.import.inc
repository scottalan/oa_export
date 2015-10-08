<?php

require_once 'oa_export.entity.import.inc';

/**
 * Helper function to import entity fields.
 *
 * @param object $entity
 *   The fully loaded entity.
 * @param string $entity_type
 *   The type of entity, e.g., 'node', 'taxonomy_term', etc.
 * @param string $key
 *   Entities are keyed by [ENTITY_TYPE:ENTITY_ID].
 * @param array $imports
 *   Contains all entities that will be imported.
 * @param array $map
 *   Contains a record of imported entities and their ids.
 *   -  [ENTITY_TYPE:OLD_ENTITY_ID] = [NEW_ENTITY_ID];
 */
function oa_export_fields_import(&$entity, $entity_type, $key, &$imports, &$map) {
  foreach (field_info_fields() as $field_name => $field) {
    if (!empty($entity->$field_name)) {
      // Here call hook functions. Doesn't use module_invoke because we
      // want to pass the clone by reference.
      foreach (module_implements('oa_import_field_' . $field['type']) as $module) {
        $function = $module . '_oa_import_field_' . $field['type'];
        $function($entity, $entity_type, $field_name, $key, $imports, $map);
      }
    }
  }
}

/**
 * Helper function to copy entity fields.
 *
 * @param object $source
 *   The imported source entity.
 * @param object $dest
 *   The destination entity to receive the fields from source
 */
function oa_export_copy_field_data($source, $dest) {
  foreach (field_info_fields() as $field_name => $field) {
    if (!empty($source->$field_name)) {
      $dest->$field_name = $source->$field_name;
    }
  }
}

/**
 * Implements hook_oa_import_field_FIELD_TYPE().
 *
 * @param object $entity
 *   The fully loaded entity.
 * @param string $entity_type
 *   The type of entity, e.g., 'node', 'taxonomy_term', etc.
 * @param string $field_name
 *   The name of the field.
 * @param string $key
 *   Entities are keyed by [ENTITY_TYPE:ENTITY_ID].
 * @param array $imports
 *   Contains all entities that will be imported.
 * @param array $map
 *   Contains a record of imported entities and their ids.
 *   -  [ENTITY_TYPE:OLD_ENTITY_ID] = [NEW_ENTITY_ID];
 * @throws EntityMalformedException
 */
function oa_export_oa_import_field_entityreference(&$entity, $entity_type, $field_name, $key, &$imports, &$map) {
  foreach ($entity->{$field_name} as $language => $targets) {
    foreach ($targets as $delta => $target) {
      // Look for the imported entity in our map.
      $node_key = 'node' . ':' . $target['target_id'];
      if (isset($map[$node_key])) {
        $nid = is_numeric($map[$node_key]) ? $map[$node_key] : $map[$node_key]->nid;
        $entity->{$field_name}[$language][$delta]['target_id'] = $nid;
      }
      else {
        // Import the entity reference now.
        $new_id = oa_export_entity_import('node', $imports[$node_key], $node_key, $imports, $map);
        // Set the new target id.
        if (!empty($new_id)) {
          $entity->{$field_name}[$language][$delta]['target_id'] = $new_id;
        }
      }
    }
  }
}

/**
 * Implements hook_oa_import_field_FIELD_TYPE().
 *
 * @param object $entity
 *   The fully loaded entity.
 * @param string $entity_type
 *   The type of entity, e.g., 'node', 'taxonomy_term', etc.
 * @param string $field_name
 *   The name of the field.
 * @param string $key
 *   Entities are keyed by [ENTITY_TYPE:ENTITY_ID].
 * @param array $imports
 *   Contains all entities that will be imported.
 * @param array $map
 *   Contains a record of imported entities and their ids.
 *   -  [ENTITY_TYPE:OLD_ENTITY_ID] = [NEW_ENTITY_ID];
 */
function oa_export_oa_import_field_paragraphs(&$entity, $entity_type, $field_name, $key, &$imports, &$map) {
  foreach ($entity->{$field_name} as $language => $items) {
    foreach ($items as $delta => &$item) {

      // Look for the imported entity in our map.
      // The paragraph items from the export are stored as integer values.
      if (isset($imports['paragraphs_item:' . $item['value']])) {

        // This has the info we need to create a new paragraph entity.
        $current_item = $imports['paragraphs_item:' . $item['value']];

        // Can only pass a variable by reference.
        $new_item = array();
        $item_entity = paragraphs_field_get_entity($new_item, $current_item->bundle, $current_item->field_name);

        $fields = field_info_instances('paragraphs_item', $current_item->bundle);

        foreach ($fields as $name => $field) {
          // Add the fields for the paragraph item.
          $item_entity->{$name} = $current_item->{$name};
        }

        // Clear the old item values.
        $entity->{$field_name}[$language][$delta] = array();

        // Set it to the new entity.
        $entity->{$field_name}[$language][$delta]['entity'] = $item_entity;

        // We don't have the new item_id until the parent entity is saved but we
        // still add the key in case we need to check for for the entity later.
        $map['paragraphs_item:' . $item['value']] = '';
      }
    }
  }
}

/**
 * Implements hook_oa_import_field_FIELD_TYPE().
 *
 * @param object $entity
 *   The fully loaded entity.
 * @param string $entity_type
 *   The type of entity, e.g., 'node', 'taxonomy_term', etc.
 * @param string $field_name
 *   The name of the field.
 * @param string $key
 *   Entities are keyed by [ENTITY_TYPE:ENTITY_ID].
 * @param array $imports
 *   Contains all entities that will be imported.
 * @param array $map
 *   Contains a record of imported entities and their ids.
 *   -  [ENTITY_TYPE:OLD_ENTITY_ID] = [NEW_ENTITY_ID];
 */
function oa_export_oa_import_field_file(&$entity, $entity_type, $field_name, $key, &$imports, &$map) {
  foreach ($entity->{$field_name} as $language => $targets) {
    foreach ($targets as $delta => $target) {
      // Look for the imported entity in our map.
      $file_key = 'file:' . $target['fid'];
      if (isset($map[$file_key])) {
        $fid = is_numeric($map[$file_key]) ? $map[$file_key] : $map[$file_key]->fid;
        $entity->{$field_name}[$language][$delta]['fid'] = $fid;
      }
      else {
        // Import the file now.
        $new_id = oa_export_entity_import('file', $imports[$file_key], $file_key, $imports, $map);
        // Set the new file id.
        if (!empty($new_id)) {
          $entity->{$field_name}[$language][$delta]['fid'] = $new_id;
        }
      }
    }
  }
}

/**
 * Implements hook_oa_import_field_FIELD_TYPE().
 *
 * @param object $entity
 *   The fully loaded entity.
 * @param string $entity_type
 *   The type of entity, e.g., 'node', 'taxonomy_term', etc.
 * @param string $field_name
 *   The name of the field.
 * @param string $key
 *   Entities are keyed by [ENTITY_TYPE:ENTITY_ID].
 * @param array $imports
 *   Contains all entities that will be imported.
 * @param array $map
 *   Contains a record of imported entities and their ids.
 *   -  [ENTITY_TYPE:OLD_ENTITY_ID] = [NEW_ENTITY_ID];
 */
function oa_export_oa_import_field_image(&$entity, $entity_type, $field_name, $key, &$imports, &$map) {
  foreach ($entity->{$field_name} as $language => $targets) {
    foreach ($targets as $delta => $target) {
      // Look for the imported entity in our map.
      $file_key = 'file:' . $target['fid'];
      if (isset($map[$file_key])) {
        $fid = is_numeric($map[$file_key]) ? $map[$file_key] : $map[$file_key]->fid;
        $entity->{$field_name}[$language][$delta]['fid'] = $fid;
      }
      else {
        // Import the file now.
        $new_id = oa_export_entity_import('file', $imports[$file_key], $file_key, $imports, $map);
        // Set the new file id. @todo: Would something like this be cleaner with a $wrapper?
        if (!empty($new_id)) {
          $entity->{$field_name}[$language][$delta]['fid'] = $new_id;
        }
      }
    }
  }
}

/**
 * Implements hook_oa_import_field_FIELD_TYPE().
 *
 * @param object $entity
 *   The fully loaded entity.
 * @param string $entity_type
 *   The type of entity, e.g., 'node', 'taxonomy_term', etc.
 * @param string $field_name
 *   The name of the field.
 * @param string $key
 *   Entities are keyed by [ENTITY_TYPE:ENTITY_ID].
 * @param array $imports
 *   Contains all entities that will be imported.
 * @param array $map
 *   Contains a record of imported entities and their ids.
 *   -  [ENTITY_TYPE:OLD_ENTITY_ID] = [NEW_ENTITY_ID];
 */
function oa_export_oa_import_field_taxonomy_term_reference(&$entity, $entity_type, $field_name, $key, &$imports, &$map) {
  foreach ($entity->{$field_name} as $language => $targets) {
    foreach ($targets as $delta => $target) {
      // Look for the imported entity in our map.
      $term_key = 'taxonomy_term:' . $target['tid'];
      if (isset($map[$term_key])) {
        $tid = is_numeric($map[$term_key]) ? $map[$term_key] : $map[$term_key]->tid;
        $entity->{$field_name}[$language][$delta]['tid'] = $tid;
      }
      else {
        // Import the file now.
        $new_id = oa_export_entity_import('taxonomy_term', $imports[$term_key], $term_key, $imports, $map);
        // Set the new file id.
        if (!empty($new_id)) {
          $entity->{$field_name}[$language][$delta]['tid'] = $new_id;
        }
      }
    }
  }
}
