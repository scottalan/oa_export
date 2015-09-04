<?php

/**
 * Helper function to export comments on entities.
 *
 * @param integer $entity_id
 * @param array $results
 */
function oa_export_entity_comments_export($entity_id, &$results) {
  foreach (oa_export_get_comments($entity_id) as $cid) {
    if ($comment = reset(entity_load('comment', array($cid)))) {
      oa_export_entity_export('comment', $comment, $results);
    }
    // Not sure this can happen but have had issues with missing entities.
    else {
      $results['messages'][] = t("The comment with cid: !cid, couldn't be exported. It no longer exists", array('!cid' => $cid));
    }
  }
}

/**
 * Export menu links for this entity.
 *
 * @param object $entity
 *   The fully loaded entity.
 * @param string $entity_type
 *   The type of entity, e.g., 'node', 'taxonomy_term', etc.
 * @param array $results
 *   Data stored in $context['results'] during the batch process.
 */
function oa_export_entity_menu_export(&$entity, $entity_id, $entity_type, &$results) {

  // Look for a link on this node entity.
  if ($mlid = og_menu_single_get_link_mlid('node', $entity_id)) {
    // Load the link by the menu link id.
    $link = menu_link_load($mlid);
    // Can there be a tree?
    $tree = _menu_build_tree(OG_MENU_SINGLE_MENU_NAME, array('expanded' => array($mlid), 'min_depth' => $link['depth']+1));

    // Save the link keyed as: link:[MENU LINK ID]:[ENTITY ID]
    $results['export']['link:' . $link['mlid']] = $link;
    $results['export']['menu_trees'] = $tree;
  }
}

/**
 * Export group metadata for this entity.
 *
 * @param object $entity
 *   The fully loaded entity.
 * @param string $entity_type
 *   The type of entity, e.g., 'node', 'taxonomy_term', etc.
 * @param array $results
 *   Data stored in $context['results'] during the batch process.
 * @throws EntityMalformedException
 */
function oa_export_entity_group_metadata_export(&$entity, $entity_type, &$results) {

  list($entity_id,,$bundle) = entity_extract_ids($entity_type, $entity);

  if ($roles = og_roles('node', $bundle, $entity_id)) {
    // Store the roles keyed by role:[BUNDLE]:[ENTITY_ID].
    $results['export']['role:' . $bundle . ':' . $entity_id] = $roles;
    if ($permissions = og_role_permissions($roles)) {
      $results['export']['permission:' . $entity_id] = $permissions;
    }
  }
}

/**
 * Export panelizer objects for this entity.
 *
 * @param object $entity
 *   The fully loaded entity.
 * @param string $entity_type
 *   The type of entity, e.g., 'node', 'taxonomy_term', etc.
 * @param int $entity_id
 *   The id of the entity being exported.
 * @param array $results
 *   Data stored in $context['results'] during the batch process.
 * @throws EntityMalformedException
 */
function oa_export_entity_panelizer_export(&$entity, $entity_type, $entity_id, &$results) {

  // Make sure the enitity is panelized. We only care about non-default view modes.
  if (isset($entity->panelizer) && !empty($entity->panelizer['page_manager'])) {

    // Get the bundle this entity belongs to.
    list(, , $bundle) = entity_extract_ids($entity_type, $entity);

    $display = &$entity->panelizer['page_manager']->display;

    foreach ($display->panels as $panel_type => $panes) {
      foreach ((array)$panes as $position => $pid) {
        // Just in case the pane isn't found.
        if (!isset($display->content[$pid])) {
          continue;
        }
        // Check for styles on the content.
        oa_export_entity_panelizer_stylizer_export($display, $pid, $entity, $entity_type, $entity_id, $results);

        // There could be custom views that belong to the panelized entity.
        oa_export_entity_panelizer_view_panes_export($display, $pid, $entity, $entity_id, $results);
      }
    }
  }
}

/**
 * Export stylizers used in panelizer objects for this entity.
 *
 * @param object $display
 *   The panelizer view mode display object.
 * @param int $pid
 *  The panel id of the panelizer content.
 * @param object $entity
 *   The fully loaded entity.
 * @param int $entity_id
 *   The id of the entity being exported.
 * @param array $results
 *   Data stored in $context['results'] during the batch process.
 */
function oa_export_entity_panelizer_stylizer_export(&$display, $pid, $entity, $entity_type, $entity_id, &$results) {
  if (!empty($display->content[$pid]->style) && isset($display->content[$pid]->style['style'])) {
    list($type, $name) = explode(':', $display->content[$pid]->style['style']);
    if ($type == 'stylizer' && isset($name)) {

      // Load all stylizer items.
      ctools_include('export');
      ctools_include('stylizer');
      $items = ctools_export_crud_load_all('stylizer');

      // Make sure we have a stylizer by that name.
      if (!empty($items[$name])) {
        // We may have already exported this stylizer.
        if (!isset($results['export']['stylizer:' . $name])) {
          // Store the stylizer in our export.
          $results['export']['stylizer:' . $name]['item'] = $items[$name];
        }
        // Instead of exporting the same stylizer multiple times we just create a place to store all entity ids
        // of entities that are panelized and contain this style.
        $results['export']['stylizer:' . $name]['entities'][] = $entity_id;
      }
      else {
        // Not sure this can happen but if we don't find a stylizer by that name: log it to watchdog and remove
        // the style so we don't get an error on import.
        $message = t('Could not find the !key stylizer during export. Removed it from the !panelizer_name content display style for the entity with id: !pid.');
        $variables = array(
          '!key' => $display->content[$pid]->style['style'],
          '!panelizer_name' => $entity->panelizer['page_manager']->name,
          '!pid' => $pid,
        );
        $link = l(t('Stylizers'), 'admin/structure/stylizer');
        watchdog('oa_export', $message, $variables, 'WATCHDOG_WARNING', $link);

        // Need to look in results and remove the style that no longer exists since the entity has already been added.
        unset($results[$entity_type . ':' . $entity_id]->panelizer['page_manager']->display->content[$pid]->style['style']);
      }
    }
  }
}

/**
 * Export custom views panes used in panelizer objects for this entity.
 *
 * @param object $display
 *   The panelizer view mode display object.
 * @param int $pid
 *  The panel id of the panelizer content.
 * @param object $entity
 *   The fully loaded entity.
 * @param int $entity_id
 *   The id of the entity being exported.
 * @param array $results
 *   Data stored in $context['results'] during the batch process.
 */
function oa_export_entity_panelizer_view_panes_export(&$display, $pid, $entity, $entity_id, &$results) {

  // Make sure this is a views pane and that it is being shown.
  // @todo: If it's not being shown do we need to remove the reference to it?
  if ($display->content[$pid]->type == 'views_panes' && $display->content[$pid]->shown) {
    // This content is a views pane so we need to export the view. subtype = "oa_project_calendar-oa_calendar_month"
    $subtype = $display->content[$pid]->subtype;
    // Get the name of the view.
    list($view_name) = explode('-', $subtype);
    // Load the view by its name.
    $view = views_get_view($view_name, TRUE);

    // If the view hasn't already been exported, export it now. We could possibly be exporting a lot of views that are
    // already created when OA is installed. It's easy to check for an existing view on import but this could increase
    // the size of the export without needing to.
    if (!isset($results['export']['view:' . $view_name])) {
      // Export the view with our results.
      $results['export']['view:' . $view_name]['item'] = $view;
    }
    // Instead of exporting the same views pane multiple times we just create a place to store all entity ids
    // of entities that are using this view pane. I don't really see any way around this for now. I guess we can worry
    // about this if the export ever grows too large?
    $results['export']['view:' . $view_name]['entities'][] = $entity_id;

    // @todo: Do we need to make sure the view exists like we do for stylizer? Can a view be deleted and its reference
    // not be removed from the display?
  }
}
