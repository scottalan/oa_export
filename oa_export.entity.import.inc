<?php

require_once 'oa_export.fields.import.inc';
require_once 'oa_export.extras.import.inc';

/**
 * Helper function to import entities.
 *
 * @param string $entity_type
 *   The type of entity, e.g., 'node', 'taxonomy_term', etc.
 * @param object $entity
 *   The fully loaded entity.
 * @param string $key
 *   Entities are keyed by [ENTITY_TYPE:ENTITY_ID].
 * @param array $imports
 *   Contains all entities that will be imported.
 * @param array $map
 *   Contains a record of imported entities and their ids.
 *   -  [ENTITY_TYPE:OLD_ENTITY_ID] = [NEW_ENTITY_ID];
 *
 * @return integer
 *   The entity id of the entity that was imported.
 */
function oa_export_entity_import($entity_type, &$entity, $key, &$imports, &$map) {

  $return = FALSE;

  // Make sure this entity hasn't been imported already.
  if (!isset($entity->oa_imported)) {
    foreach (module_implements('oa_import_entity_' . $entity_type) as $module) {
      $function = $module . '_oa_import_entity_' . $entity_type;
      $function($entity, $key, $imports, $map);
    }

    // If the entity if fieldable check for fields that may need to be imported.
    if (entity_type_is_fieldable($entity_type)) {
      oa_export_fields_import($entity, $entity_type, $key, $imports, $map);
    }

    // Allow anyone to alter the clone one last time.
    $context = array(
      'entity_type' => $entity_type,
      'key' => $key,
      'imports' => $imports,
      'map' => $map,
    );
    drupal_alter('oa_import_clone_entity', $entity, $context);
  }

  // Check again. In the case of a 'file', for example, we save the entity a
  // different way and mark it as imported.
  if (!isset($entity->oa_imported)) {
    // Flag the entity as new.
    $entity->is_new = TRUE;
    // Save the entity.
    entity_save($entity_type, $entity);

    // Get the original entity id before the entity was saved.
    list(,$orig_entity_id) = explode(':', $key);

    // We need to check to see if the entity is panelized and if so import it.
    oa_export_entity_panelizer_import($entity, $entity_type, $orig_entity_id);

    // Mark the entity as imported.
    $entity->oa_imported = TRUE;
  }

  // Get the entity's new id and return it.
  list($entity_id) = entity_extract_ids($entity_type, $entity);
  if (isset($entity_id)) {
    $return = $entity_id;
  }
  return $return;
}

/**
 * Implements hook_oa_import_entity_ENTITY_TYPE().
 *
 * @param object $entity
 *   The fully loaded entity.
 * @param string $key
 *   Entities are keyed by [ENTITY_TYPE:ENTITY_ID].
 * @param array $imports
 *   Contains all entities that will be imported.
 * @param array $map
 *   Contains a record of imported entities and their ids.
 *   -  [ENTITY_TYPE:OLD_ENTITY_ID] = [NEW_ENTITY_ID];
 */
function oa_export_oa_import_entity_node(&$entity, $key, &$imports, &$map) {
  if (!isset($map[$key])) {
    global $user;
    $entity->nid = NULL;
    $entity->uid = $user->uid;
    $entity->tnid = NULL;
    $entity->vid = NULL;
    $entity->created = NULL;
    $entity->changed = NULL;
    $entity->path = NULL;
    $entity->revision_timestamp = NULL;
  }
  else {
    // Load the entity that was already imported.
    $entity = entity_load_single('node', $map[$key]);
  }
}

/**
 * Implements hook_oa_import_entity_ENTITY_TYPE().
 *
 * @param object $entity
 *   The fully loaded entity.
 * @param string $key
 *   Entities are keyed by [ENTITY_TYPE:ENTITY_ID].
 * @param array $imports
 *   Contains all entities that will be imported.
 * @param array $map
 *   Contains a record of imported entities and their ids.
 *   -  [ENTITY_TYPE:OLD_ENTITY_ID] = [NEW_ENTITY_ID];
 */
function oa_export_oa_import_entity_paragraphs_item(&$entity, $key, &$imports, &$map) {
  // Don't save this entity. It will be saved when the entity it is attached to
  // is saved.
  $entity->oa_imported = TRUE;
}

/**
 * Implements hook_oa_import_entity_ENTITY_TYPE().
 *
 * @param object $entity
 *   The fully loaded entity.
 * @param string $key
 *   Entities are keyed by [ENTITY_TYPE:ENTITY_ID].
 * @param array $imports
 *   Contains all entities that will be imported.
 * @param array $map
 *   Contains a record of imported entities and their ids.
 *   -  [ENTITY_TYPE:OLD_ENTITY_ID] = [NEW_ENTITY_ID];
 */
function oa_export_oa_import_entity_taxonomy_term(&$entity, $key, &$imports, &$map) {
  $exists = FALSE;
  $terms = taxonomy_get_term_by_name($entity->name);
  foreach ($terms as $tid => $term) {
    // We found an existing term.
    if ($entity->vid == $term->vid) {
      $exists = TRUE;
      continue;
    }
  }
  // If this is not a Blueprint then we don't want to create another term with the same name. This keeps us from
  // re-importing taxonomy, for example, that OA defines by default. This term would not be a blueprint.
  // @todo: If a taxonomy term that was exported just happens to have the same tid as one that already exists this
  // @todo: could still be an issue. Need to think this out and maybe we need to compare all columns in the database:
  // @todo: tid, vid, name, description to make sure this doesn't just happen to have the same name but a different description.
  if ($exists && empty($term->field_oa_clone_space)) {
    // Just pretend we already imported it.
    $entity->oa_imported = TRUE;
  }
  else {
    $entity->tid = NULL;
    // Make sure we have the correct vocabulary vid.
    $vocab = taxonomy_vocabulary_machine_name_load($entity->vocabulary_machine_name);
    // Associate this taxonomy term with the correct vocabulary.
    $entity->vid = $vocab->vid;
  }
}

/**
 * Implements hook_oa_import_entity_ENTITY_TYPE().
 *
 * @param object $entity
 *   The fully loaded entity.
 * @param string $key
 *   Entities are keyed by [ENTITY_TYPE:ENTITY_ID].
 * @param array $imports
 *   Contains all entities that will be imported.
 * @param array $map
 *   Contains a record of imported entities and their ids.
 *   -  [ENTITY_TYPE:OLD_ENTITY_ID] = [NEW_ENTITY_ID];
 */
function oa_export_oa_import_entity_comment(&$entity, $key, &$imports, &$map) {

  // See if the entity has already been imported. As of now comments can only
  // be added to 'node' entities.
  if (isset($map['node:' . $entity->nid])) {
    $entity_id = $map['node:' . $entity->nid];
  }
  else {
    // Import the entity now.
    $entity_id = oa_export_entity_import('node', $imports['node:' . $entity->nid], 'node:' . $entity->nid, $imports, $map);
  }
  // Set the new comment nid.
  $entity->cid = NULL;
  $entity->created = NULL;
  $entity->changed = NULL;
  $entity->nid = $entity_id;
}

/**
 * Implements hook_oa_import_entity_ENTITY_TYPE().
 *
 * @param object $entity
 *   The fully loaded entity.
 * @param string $key
 *   Entities are keyed by [ENTITY_TYPE:ENTITY_ID].
 * @param array $imports
 *   Contains all entities that will be imported.
 * @param array $map
 *   Contains a record of imported entities and their ids.
 *   -  [ENTITY_TYPE:OLD_ENTITY_ID] = [NEW_ENTITY_ID];
 *
 * We need to not only create a new entry for the file but we need to copy the
 * file to it's new location as well. This accomplishes both.
 */
function oa_export_oa_import_entity_file(&$entity, $key, &$imports, &$map) {

  // The files that were exported are located here.
  $file_location = $_SESSION['oa_export']['extract_location'] . '/files';

  // Make sure our file entity is an array.
  $file = is_object($entity) ? (array) $entity : $entity;

  // Location where the file will be saved.
  $destination = drupal_dirname($file['uri']);

  // The name of the file.
  $filename = drupal_basename($file['uri']);

  // Find the actual file we need to import.
  $file_contents = file_get_contents(realpath($file_location.DIRECTORY_SEPARATOR.$filename));

  // Save the file by the same name to its new location. This calls file_save()
  // for us.
  if ($new_file = file_save_data($file_contents, $destination . $filename)) { // FILE_EXISTS_REPLACE
    // Convert the file back to an object after merging the old file with the
    // new file. This is necessary as the fields that exist on the old file are
    // not available on the new file.
    $entity = (object) array_merge($file, (array) $new_file);

    // Add the imported file to our map.
    $map['file:' . $file['fid']] = $entity->fid;

    // This will stop the file from being saved a second time.
    $entity->oa_imported = TRUE;
  }
}
