<?php

require_once 'oa_export.batch.export.inc';

function oa_export_blueprint_export_form($form, &$form_state, $taxonomy) {

  // Temporary directory for system.
  $tmp_dir = sys_get_temp_dir();
  if (!is_writable($tmp_dir)) {
    drupal_set_message(t('In order for this export to work "%dir" needs to be writable.', array('%dir' => $tmp_dir)), 'error');
    return $form;
  }

  // If this blueprint is 'cloneable' then we can get the space id.
  $space_id = !empty($taxonomy->field_oa_clone_space) ? $taxonomy->field_oa_clone_space[LANGUAGE_NONE][0]['target_id'] : FALSE;

  if ($space_id) {
    // Load the space.
    $space = node_load($space_id);

    // Get the full blueprint so we can export it.
    $blueprint = taxonomy_term_load($taxonomy->tid);
//    $subspaces = oa_core_get_groups_by_parent($space_id, OA_SPACE_TYPE);
//    $sections = oa_core_get_groups_by_parent($space_id, OA_SECTION_TYPE);
//    $groups = oa_core_get_groups_by_parent($space_id, OA_GROUP_TYPE);
//    $teams = oa_core_get_groups_by_parent($space_id, OA_TEAM_TYPE);

    drupal_set_title(t('Export a blueprint for "!space"', array('!space' => $space->title)));

    $form['blueprint'] = array(
      '#type' => 'hidden',
      '#value' => $taxonomy->tid,
    );
    $form['space'] = array(
      '#type' => 'hidden',
      '#value' => $space_id,
    );

    $form['actions'] = array(
      '#type' => 'actions',
      'submit' => array(
        '#type' => 'submit',
        '#value' => t('Export'),
      ),
    );
  }
  else {
    drupal_set_message(t("Looks like you can't export this Blueprint."), 'error');
    $form['error'] = array(
      '#markup' => '<p>' . t("Looks like you can't export this Blueprint.") . '</p>'
    );
  }

  return $form;
}

function oa_export_blueprint_export_form_validate($form, &$form_state) {
  // Make sure we have a term id for the blueprint and a nid for the space.
  if (empty($form_state['values']['blueprint']) || empty($form_state['values']['space'])) {
    drupal_set_message(t("We can't seem to find the Blueprint or Space for your export."), 'error');
    form_set_error('', NULL);
  }
}

function oa_export_blueprint_export_form_submit($form, &$form_state) {

  $blueprint = taxonomy_term_load($form_state['values']['blueprint']);
  // We need the original space.
  $space = node_load($form_state['values']['space']);

  oa_export_batch_export($blueprint, $space);
  // Redirect the user to a page to download the file.
  batch_process('oa_export/download');

}
