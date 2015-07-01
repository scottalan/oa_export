<?php

require_once 'oa_export.utils.inc';

/**
 * Sets up a batch to export a blueprint.
 *
 * @param $blueprint
 * @param $space
 */
function oa_export_batch_export($blueprint, $space) {

  $batch = array(
    'title' => t('Exporting %type to file...', array('%type' => 'Blueprint')),
    'finished' => 'oa_export_batch_export_finished',
  );

  // Store space data in session.
  $_SESSION['oa_export'] = array();

  // Store the path we will link to after download.
  $_SESSION['oa_export']['redirect'] = $_GET['q'];

  // Store the export directory in the session variable.
  $_SESSION['oa_export']['directory'] = oa_export_create_temp_directory();

  // Setup a directory for exporting files.
  $_SESSION['oa_export']['files_directory'] = oa_export_create_sub_directory($_SESSION['oa_export']['directory'], 'files');

  // Defines operations that are added to the batch.
  _oa_export_batch_blueprint($batch, $blueprint, $space);

  // Add the batch.
  batch_set($batch);
}

/**
 * Exports a blueprint via a .tar.gz file.
 *
 * @param $batch
 * @param $blueprint
 * @param $space
 */
function _oa_export_batch_blueprint(&$batch, $blueprint, $space) {

  // Export a json file that contains the blueprint.
  $export = oa_export_create_json_export('blueprint', $blueprint, $_SESSION['oa_export']['directory']);

  // @todo: Add this in the finish function.
  $_SESSION['oa_export']['blueprint'] = $blueprint;

  // Add some operations to our batch process.
  oa_export_batch_operations($batch, $space);
}

/**
 * Helper that defines the batch operations.
 *
 * @param $batch
 * @param $space
 */
function oa_export_batch_operations(&$batch, $space, $is_subspace = FALSE) {

  $bypass_access_check = TRUE;

  // Export space.
  $batch['operations'][] = array('_oa_export_batch_space', array($space));

//  $groups = oa_core_get_groups_by_parent($space_id, OA_GROUP_TYPE);
//  $batch['operations'][] = array('_oa_export_batch_group_memberships', array($space));

  // Clone the OG metadata.
//  $batch['operations'][] = array('_oa_export_batch_group_metadata', array($space));
  $not_orphan = array();

  // Clone all the Sections in this Space.
  $sections = array_keys(oa_core_space_sections($space->nid, 1, $bypass_access_check));
  foreach ($sections as $original_section_nid) {
    $batch['operations'][] = array('_oa_export_batch_section', array($space, $original_section_nid));
//    $not_orphan[$original_section_nid] = $original_section_nid;
//      $not_orphan = array_merge($not_orphan, oa_clone_get_section_content($original_section_nid, $bypass_access_check));
    $content = oa_clone_get_section_content($original_section_nid, $bypass_access_check);
    if (!empty($content)) {
      $batch['operations'][] = array('_oa_export_batch_section_content', array($space, $original_section_nid));
    }
  }

  if (module_exists('oa_subspaces')) {
    // Clone all sub-Spaces in this Space.
    $associated_entities = oa_core_get_groups_by_parent($space->nid);
    $other = oa_core_get_groups_by_parent(0);
    if (!empty($associated_entities)) {
      foreach ($associated_entities as $original_subspace_nid) {
        $not_orphan[$original_subspace_nid] = $original_subspace_nid;
        if ($subspace = node_load($original_subspace_nid)) {
          oa_export_batch_operations($batch, $subspace, TRUE);
        }
      }
    }
  }

  // Find orphan content for this space.
  $orphan = oa_clone_get_orphan_content($space->nid, $bypass_access_check);

  if ($nids = array_diff($orphan, $not_orphan)) {
    $batch['operations'][] = array('_oa_export_batch_orphan_content', array($space, $nids));
  }
  $batch['operations'][] = array('_oa_export_batch_og_menu', array($space));
}

/**
 * Exports the space for our blueprint.
 *
 * @param object $space
 * @param array $context
 */
function _oa_export_batch_space($space, &$context) {

  if (empty($context['sandbox']['max'])) {
    $context['sandbox']['progress'] = 0;
    $context['sandbox']['max'] = 1;
    $context['results']['total'] = (!empty($context['results']['total']) ? $context['results']['total'] : 0) + $context['sandbox']['max'];
  }

  // Find orphan content for this space.
  $orphaned = oa_clone_get_orphan_content($space->nid, TRUE);

  // Determines if this is a top-level space or a subspace.
  $identifier = !empty($space->oa_parent_space) ? 'subspace:' : 'space:';

  // Add the space to the results.
  $context['results']['blueprint'][$identifier . $space->nid]['entity'] = $space;

  // Look for files we need to export. This will get files for the space. We
  // will check for files related to paragraphs later.
  if ($files = oa_export_get_media_files($space, $context)) {
    $context['results']['blueprint'][$identifier . $space->nid]['files'] = $files;
  }

  // We need to check for paragraph items and export those as well. Paragraphs
  // runs hook_node_clone_alter() but we have to override it on import. We need
  // information about the paragraph items on import so we can create new ones.
  // We also need to check for any fields that are type == 'paragraph' not just
  // field_oa_related.
  if (isset($space->field_oa_related) && !empty($space->field_oa_related[LANGUAGE_NONE])) {
    $items = $space->field_oa_related[LANGUAGE_NONE];

    $paragraphs = oa_export_export_paragraph_items($items, 'node', $identifier . $space->nid, $context);

    if (!empty($paragraphs)) {
      $context['results']['blueprint'][$identifier . $space->nid]['paragraphs'] = $paragraphs;
    }

  }

  // See if there are any comments associated with this space.
  $comments = oa_export_find_comments($space->nid);

  if (!empty($comments)) {
    foreach ($comments as $cid) {

      // Load the full comment.
      $comment = comment_load($cid);

      $context['results']['blueprint'][$identifier . $space->nid]['comments']['comment:' . $cid]['entity'] = $comment;

      // We need to check for paragraph items and export those as well. Paragraphs
      // runs hook_node_clone_alter() but we have to override it on import. We need
      // information about the paragraph items on import so we can create new ones.
      // We just store the whole entity so we have access to what we need later.
      if (isset($comment->field_oa_related) && !empty($comment->field_oa_related[LANGUAGE_NONE])) {
        $items = $comment->field_oa_related[LANGUAGE_NONE];

        $comment_paragraphs = oa_export_export_paragraph_items($items, 'comment', 'comment:' . $cid, $context);

        $context['results']['blueprint'][$identifier . $space->nid]['comments']['comment:' . $cid]['paragraphs'] = $comment_paragraphs;

      }
    }
  }

  // Bump the progress indicator.
  $context['sandbox']['progress']++;

  // We are finished with a single space.
  $context['finished'] = $context['sandbox']['progress'] / $context['sandbox']['max'];
}

/**
 * Exports each section for a space.
 *
 * Fields to check for:
 * - field_oa_related
 * - field_oa_section
 *   Has tid. Could this be different if the user has somehow changed them?
 *
 * @TODO: Maybe we don't need to nest everything under e.g., 'space:2'. How about: $context['results']['export']
 * then space, files, paragraphs, comments, sections, subspaces, etc. Then as we
 * are importing we just look in each directory and locate the 'item' we need. Most
 * of them have a key we can find the 'nid' of the node we are currently importing.
 * As of now we would need to add 'entity => $nid' for each paragraph:ID but comments
 * show which nid it goes to, sections have 'og_group_ref' for the space it belongs to.
 * Files don't have a reference in their array but we could add a key that wraps the
 * 'fid' => $file array with the nid of node it belongs to if we need to.
 *
 * @param object $space
 *   The space the section belongs to.
 * @param int $section_id
 *   The nid of the section we are exporting.
 * @param array $context
 */
function _oa_export_batch_section($space, $section_id, &$context) {

  if (empty($context['sandbox']['max'])) {
    $context['sandbox']['progress'] = 0;
    $context['sandbox']['max'] = 1;
    $context['results']['total'] = (!empty($context['results']['total']) ? $context['results']['total'] : 0) + $context['sandbox']['max'];
  }

  // Find orphan content for this section.
  $orphaned = oa_clone_get_orphan_content($section_id, TRUE);

  // Determines if this is a top-level space or a subspace.
  $identifier = !empty($space->oa_parent_space) ? 'subspace:' : 'space:';

  $section = node_load($section_id);

  // Add the section to our result.
  $context['results']['blueprint'][$identifier . $space->nid]['content']['section:' . $section_id]['entity'] = $section;

  // Look for files we need to export.
  $files = oa_export_get_media_files($section, $context);

  // Add files for the section to our result.
  if (!empty($files)) {
    $context['results']['blueprint'][$identifier . $space->nid]['content']['section:' . $section_id]['files'] = $files;
  }

  // We need to check for paragraph items and export those as well. Paragraphs
  // runs hook_node_clone_alter() but we have to override it on import. We need
  // information about the paragraph items on import so we can create new ones.
  // We just store the whole entity so we have access to what we need later.
  if (isset($section->field_oa_related) && !empty($section->field_oa_related[LANGUAGE_NONE])) {
    $items = $section->field_oa_related[LANGUAGE_NONE];

    $paragraphs = oa_export_export_paragraph_items($items, 'node', 'section:' . $section_id, $context);

    // Add paragraphs for the section to our result.
    if (!empty($paragraphs)) {
      $context['results']['blueprint'][$identifier . $space->nid]['content']['section:' . $section_id]['paragraphs'] = $paragraphs;
    }

  }

  // See if there are any comments associated with this space.
  $comments = oa_export_find_comments($section->nid);

  if (!empty($comments)) {
    foreach ($comments as $cid) {

      // Load the full comment.
      $comment = comment_load($cid);

      // Store the comment with the section.
      $context['results']['blueprint'][$identifier . $space->nid]['content']['section:' . $section_id]['comments']['comment:' . $cid]['entity'] = $comment;

      // We need to check for paragraph items and export those as well. Paragraphs
      // runs hook_node_clone_alter() but we have to override it on import. We need
      // information about the paragraph items on import so we can create new ones.
      // We just store the whole entity so we have access to what we need later.
      if (isset($comment->field_oa_related) && !empty($comment->field_oa_related[LANGUAGE_NONE])) {
        $items = $comment->field_oa_related[LANGUAGE_NONE];

        $comment_paragraphs = oa_export_export_paragraph_items($items, 'comment', 'comment:' . $cid, $context);

        $context['results']['blueprint'][$identifier . $space->nid]['content']['section:' . $section_id]['comments']['comment:' . $cid]['paragraphs'] = $comment_paragraphs;

      }
    }
  }

  // Bump the progress indicator.
  $context['sandbox']['progress']++;

  // Report progress if not finished and run again.
  $context['finished'] = $context['sandbox']['progress'] / $context['sandbox']['max'];
}

/**
 * Export section content for the space during a batch process.
 *
 * @param object $space
 *   The space we are exporting.
 * @param int $section_id
 *   The section id we are going to export content for.
 * @param array &$context
 *   A place where we can store values that need to be passed from one iteration
 *   of this batch operation to the next.
 */
function _oa_export_batch_section_content($space, $section_id, &$context) {

  if (empty($context['sandbox']['max'])) {
    $context['sandbox']['content_ids'] = oa_clone_get_section_content($section_id, TRUE);
    $context['sandbox']['max'] = count($context['sandbox']['content_ids']);
    $context['sandbox']['progress'] = 0;
    $context['results']['total'] = (!empty($context['results']['total']) ? $context['results']['total'] : 0) + $context['sandbox']['max'];
  }

  // Determines if this is a top-level space or a subspace.
  $identifier = !empty($space->oa_parent_space) ? 'subspace:' : 'space:';

  // Get the next node.
  $next_id = array_shift($context['sandbox']['content_ids']);
  if (!$next_id) {
    $context['sandbox']['finished'] = TRUE;
    return;
  }

  // The section content.
  $content = node_load($next_id);

  // Find orphan content for this section content.
  $orphaned = oa_clone_get_orphan_content($content->nid, TRUE);

  // Add the content to our results.
  $context['results']['blueprint'][$identifier . $space->nid]['content']['section:' . $section_id]['content']['section_content:' . $content->nid]['entity'] = $content;

  // Look for files we need to export for this content.
  $files = oa_export_get_media_files($content, $context);

  // Export files for section content.
  if (!empty($files)) {
    $context['results']['blueprint'][$identifier . $space->nid]['content']['section:' . $section_id]['content']['section_content:' . $content->nid]['files'] = $files;
  }

  // We need to check for paragraph items and export those as well. Paragraphs
  // runs hook_node_clone_alter() but we have to override it on import. We need
  // information about the paragraph items on import so we can create new ones.
  // We just store the whole entity so we have access to what we need later.
  if (isset($content->field_oa_related) && !empty($content->field_oa_related[LANGUAGE_NONE])) {
    $items = $content->field_oa_related[LANGUAGE_NONE];

    $paragraphs = oa_export_export_paragraph_items($items, 'node', 'section_content:' . $content->nid, $context);

    // Export paragraphs for section content.
    if (!empty($paragraphs)) {
      $context['results']['blueprint'][$identifier . $space->nid]['content']['section:' . $section_id]['content']['section_content:' . $content->nid]['paragraphs'] = $paragraphs;
    }

  }

  // Comments related to the section content.
  $comments = oa_export_find_comments($content->nid);

  if (!empty($comments)) {
    foreach ($comments as $cid) {

      // Load the full comment.
      $comment = comment_load($cid);

      // Store the comment with the section content.
      $context['results']['blueprint'][$identifier . $space->nid]['content']['section:' . $section_id]['content']['section_content:' . $content->nid]['comments']['comment:' . $cid]['entity'] = $comment;

      // We need to check for paragraph items and export those as well. Paragraphs
      // runs hook_node_clone_alter() but we have to override it on import. We need
      // information about the paragraph items on import so we can create new ones.
      // We just store the whole entity so we have access to what we need later.
      if (isset($comment->field_oa_related) && !empty($comment->field_oa_related[LANGUAGE_NONE])) {
        $items = $comment->field_oa_related[LANGUAGE_NONE];

        $comment_paragraphs = oa_export_export_paragraph_items($items, 'comment', 'comment:' . $cid, $context);

        // Export paragraphs for comments.
        if (!empty($comment_paragraphs)) {
          $context['results']['blueprint'][$identifier . $space->nid]['content']['section:' . $section_id]['content']['section_content:' . $content->nid]['comments']['comment:' . $cid]['paragraphs'] = $comment_paragraphs;
        }

      }
    }
  }

  // Bump the progress indicator.
  $context['sandbox']['progress']++;

  // Report progress if not finished and run again.
  $context['finished'] = $context['sandbox']['progress'] / $context['sandbox']['max'];
}

/**
 * @todo: Still need to test this out.
 *
 * @param object $space
 * @param array $nids
 * @param array $context
 */
function _oa_export_batch_orphan_content($space, $nids, &$context) {

  if (empty($context['sandbox']['max'])) {
    $context['sandbox']['content_ids'] = $nids;
    $context['sandbox']['max'] = count($nids);
    $context['sandbox']['progress'] = 0;
    $context['results']['total'] = (!empty($context['results']['total']) ? $context['results']['total'] : 0) + $context['sandbox']['max'];
  }

  // Determines if this is a top-level space or a subspace.
  $identifier = !empty($space->oa_parent_space) ? 'subspace:' : 'space:';

  // Get the next node.
  $next_id = array_shift($context['sandbox']['content_ids']);
  if (!$next_id) {
    $context['sandbox']['finished'] = TRUE;
    return;
  }

  // Get the original node and mark it so that it doesn't get cloned again.
  $orphan_content = node_load($next_id);
  $orphan_content->oa_clone_skip = TRUE;

  // Look for files we need to export.
  $files = oa_export_get_media_files($orphan_content, $context);

  // Bump the progress indicator.
  $context['sandbox']['progress']++;

  // Report progress if not finished and run again.
  $context['finished'] = $context['sandbox']['progress'] / $context['sandbox']['max'];

  if (!empty($orphan_content)) {
    $context['results']['blueprint'][$identifier . $space->nid]['content']['orphan_content:' . $next_id] = $orphan_content;
  }
  if (!empty($files)) {
    $context['results']['blueprint'][$identifier . $space->nid]['content']['orphan_files:' . $next_id] = $files;
  }
}

/**
 * @param $space
 * @param $context
 */
function _oa_export_batch_og_menu($space, &$context) {
  if (empty($context['sandbox']['max'])) {
    $context['sandbox']['progress'] = 0;
    $context['sandbox']['max'] = 1;
    $context['results']['total'] = (!empty($context['results']['total']) ? $context['results']['total'] : 0) + $context['sandbox']['max'];
  }

  // Determines if this is a top-level space or a subspace.
  $identifier = !empty($space->oa_parent_space) ? 'subspace:' : 'space:';

  if ($mlid = og_menu_single_get_link_mlid('node', $space->nid)) {
    $link = menu_link_load($mlid);
    $tree = _menu_build_tree(OG_MENU_SINGLE_MENU_NAME, array('expanded' => array($mlid), 'min_depth' => $link['depth']+1));
  }

  // Bump the progress indicator.
  $context['sandbox']['progress']++;

  // Report progress if not finished and run again.
  $context['finished'] = $context['sandbox']['progress'] / $context['sandbox']['max'];

  if ($context['finished']) {

    if (!empty($link)) {
      $context['results']['blueprint'][$identifier . $space->nid]['links:' . $link['mlid']] = $link;
    }
    if (!empty($tree['tree'])) {
      $context['results']['blueprint'][$identifier . $space->nid]['menu_trees'] = $tree;
    }
//    $created = oa_export_create_json_export('menu', $tree, $_SESSION['oa_export']['directory']);
  }
}

/**
 * Exports group metadata for a space.
 *
 * Thoughts...probably need to export all roles so we can look for custom roles
 * that may have been created that need to be exported.
 *
 * @param $space
 * @param $context
 */
function _oa_export_batch_group_metadata($space, &$context) {

  if (empty($context['sandbox']['role_count'])) {
    $context['sandbox']['max'] = 1;
    $context['sandbox']['progress'] = 0;
    $context['results']['total'] = (!empty($context['results']['total']) ? $context['results']['total'] : 0) + $context['sandbox']['max'];
  }

  // Determines if this is a top-level space or a subspace.
  $identifier = !empty($space->oa_parent_space) ? 'subspace:' : 'space:';

  // Export OG roles.
  $original_role_map = array();
  $original_roles = og_roles('node', $space->type, $space->nid);
  $custom_roles = array();
  foreach ($original_roles as $original_rid => $original_name) {
    $original_role_map[$original_name] = $original_rid;
    // If this is a custom role, then we have to create it.
    if (!in_array($original_name, array(OG_ANONYMOUS_ROLE, OG_AUTHENTICATED_ROLE, OG_ADMINISTRATOR_ROLE))) {
      $role = og_role_create($original_name, 'node', $space->nid, $space->type);
      $custom_roles[$role->rid] = $role;
    }
  }

  $roles = og_roles('node', $space->type, $space->nid);

  // Custom roles won't have a rid so we will just push them into the roles
  // array.
  foreach ($custom_roles as $custom_role) {
    array_push($custom_role, $roles);
  }

  // Add roles to context.
  $context['sandbox']['roles'] = $roles;

  // Add the roles for the space to our result.
  $context['results']['blueprint'][$identifier . $space->nid]['roles'] = $roles;

  // Returns TRUE if the file was created.
//  $export = oa_export_create_json_export('role', $roles, $_SESSION['oa_export']['directory']);


  // Clone OG permissions. @todo: this will need to be done on import.
//  $permissions = og_role_permissions($original_roles);
//  $roles = og_roles('node', $space->type, $space->nid);
//  foreach ($roles as $rid => $name) {
//    og_role_change_permissions($rid, $permissions[$original_role_map[$name]]);
//  }

  // Allow other modules to copy group metadata too.
//  module_invoke_all('oa_export_group_metadata', $space);
}

/**
 * Callback for cloning all the memberships in a group.
 *
 * @param object $node
 *   Node object representing the Group we are going to popuplate.
 * @param int $original_nid
 *   The node ID of the original Group we are cloning data from.
 * @param array &$context
 *   A place where we can store values that need to b passed from one iteration
 *   of this batch operation to the next.
 */
function _oa_export_batch_group_memberships($space, &$context) {

  $groups = oa_core_get_groups_by_parent($space->nid, OA_GROUP_TYPE);

  // The first time through, set up all the variables.
  if (empty($context['sandbox']['max'])) {
    $context['sandbox']['membership_ids'] = oa_clone_get_group_memberships($space->nid);
    $context['sandbox']['max'] = count($context['sandbox']['membership_ids']);
    $context['sandbox']['gid'] = $space->nid;
    $context['sandbox']['progress'] = 0;
  }

  // Determines if this is a top-level space or a subspace.
  $identifier = !empty($space->oa_parent_space) ? 'subspace:' : 'space:';

  // Get the next node.
  $next_id = array_shift($context['sandbox']['membership_ids']);
  if (!$next_id) {
    $context['sandbox']['finished'] = TRUE;
    return;
  }

  // Clone the original membership, change it to the new group and save.
  $memberships[$next_id] = og_membership_load($next_id);

  // Bump the progress indicator.
  $context['sandbox']['progress']++;

  // Report progress if not finished and run again.
  $context['finished'] = $context['sandbox']['progress'] / $context['sandbox']['max'];

  // When we have gone through all the content we write the file.
  if ($context['finished']) {

    $context['results']['blueprint'][$identifier . $space->nid]['memberships'] = $memberships;
  }
}

/**
 * Finish function for our batch.
 *
 * @param $success
 * @param $results
 * @param $operations
 */
function oa_export_batch_export_finished($success, $results, $operations) {
  if ($success) {
    // Add the initial blueprint to the results.
//    $results['blueprint']['blueprint'] = $_SESSION['oa_export']['blueprint'];

    try {
      // Generate a file name that matches our directory.
      $base = basename($_SESSION['oa_export']['directory']);
      $tmp = file_directory_temp();
      $_SESSION['oa_export']['file'] = $tmp . DIRECTORY_SEPARATOR . $base . '.tar.gz';

      // Need to iterate over arrays in $results['blueprint'].
      foreach ($results['blueprint'] as $target => $data) {
        list($type,$id) = explode(':', $target);

        // Create a json file to add to our export directory that contains the blueprint.
        $file = oa_export_create_json_export($target, $data, $_SESSION['oa_export']['directory']);

        if ($file) {
          $tarfile = $_SESSION['oa_export']['file'];

          // Build a compressed file from our blueprint.
          $new_tar = new Archive_Tar($_SESSION['oa_export']['file']);
          // We call this directly so we can modify the directory structure for our
          // tar file. This will remove "/tmp" from the tar.
          $new_tar->createModify(array($_SESSION['oa_export']['directory']), '', file_directory_temp());
//
          // Get the public file directory.
          $public = variable_get('file_public_path', conf_path() . '/files');

          // Copy the compressed file to a path we can download.
          if ($copy = file_unmanaged_copy($tarfile, $public, FILE_EXISTS_REPLACE)) {
            $_SESSION['oa_export']['download_path'] = $copy;
          }

          // Display a message telling the user the export is done.
          drupal_set_message(t('Finished exporting your Blueprint successfully.'));
        }
        else {
          drupal_set_message(t('There was an error creating the export.'), 'error');
        }
      }
    }
    catch (Exception $e) {
      oa_export_exception($e);
    }
  }
  else {
    drupal_set_message(t('Error exporting content.'), 'error');
  }
}

/**
 * Helper function that handles the redirect for our download.
 *
 * @return string
 */
function oa_export_batch_export_finished_redirect() {

  if (empty($_SESSION['oa_export']['directory']) || empty($_SESSION['oa_export']['file'])) {
    return t('Please visit your list subscribers page to begin a list download.');
  }

  // Redirect to the download file.
  $redirect = base_path() . $_SESSION['oa_export']['download_path'];
  drupal_add_js('setTimeout(function() { window.location.href = "' . $redirect . '"; }, 2000);', 'inline');

  $download_link = l(
    t('click here to download the file'),
    $_SESSION['oa_export']['download_path']
  );
  $output = '<p>' . t('Your Blueprint is ready for download. The download should begin automatically. If not, please !download_link.', array('!download_link' => $download_link)) . '</p>';
  $output .= '<p>' . l(
      t("Back to export"), // &#8592;
      $_SESSION['oa_export']['redirect'],
      array(
        'html' => TRUE,
        'attributes' => array(
          'class' => 'btn btn-default btn-primary',
        ),
      )
    ) . '</p>';

  // Get rid of our session variables.
  unset($_SESSION['oa_export']);

  return $output;
}
