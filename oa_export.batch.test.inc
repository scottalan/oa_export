<?php

require_once 'oa_export.utils.inc';
require_once 'oa_export.import.inc';

/**
 * @TODO: This is an attempt to dynamically add batch operations as we need them.
 *
 * Helper to dynamically add a batch set during a batch process.
 *
 * @param string $key
 *  The key we are targeting within the associative data array.
 * @param array $data
 *  An associative array of data.
 * @param int $original_id
 *  The original id of the entity we need to import.
 * @param int $parent_id
 *  The id of the entity this entity belongs to.
 */
function oa_export_add_batch_set($key, $data, $original_id, $parent_id) {

  // $data gives us for example in the case of a section: section, paragraphs, files.
  // $original_nid is the old nid of the node.

  // We are defining a batch for each individual entity here. So if this was for
  // sections we would only pass in a single section here.

  $target = $data[$key];

  $batch = array(
    'title' => t('Importing !type !title', array('!type' => $target->type, '!title' => $target->title)),
//    'finished' => 'oa_export_import_' . $name . '_finished',
    'finished' => 'oa_export_import_batch_finished',
    'file' => drupal_get_path('module', 'oa_export') . '/oa_export.batch.test.inc',
  );

  $batch['operations'][] = array('oa_export_batch_import_' . $key . '_test', array($target, $data, $original_id, $parent_id));

  batch_set($batch);

}

function oa_export_batch_import_space_test($import, &$context) {

  if (empty($context['sandbox']['max'])) {
    $context['sandbox']['progress'] = 0;
    $context['sandbox']['max'] = 1;
    $context['results']['total'] = (!empty($context['results']['total']) ? $context['results']['total'] : 0) + $context['sandbox']['max'];
  }

  // This is the space we are importing this blueprint for.
  $original_space = $import['space'];

  // See if we are importing a subspace.
  $parent_space = !empty($original_space->oa_parent_space) ? $original_space->oa_parent_space[LANGUAGE_NONE][0]['target_id'] : NULL;

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
  // and then relate to the new node.
//  oa_export_import_related_data($import, $context, $cloned_node, $original_space);

  // Going to try something here...Let's see what happens if we define a new
  // batch during this batch. It will insert the new set right after this one
  // (the current one) to "ensure execution order" and store its operations in
  // a queue.

  // Will move all these to a separate function to keep it clean. ^

  // Check for comments.
  if (isset($import['comment']) && !empty($import['comment']['comments'])) {
    foreach ($import['comment']['comments'] as $target => $data) {

      list($name,$original_id) = explode(':', $target);

      // This function will need to be generic enough to add a new batch whenever
      // we need to for: sections, section content, etc.
      oa_export_add_batch_set('comments', $import['comment'], NULL, $cloned_node->nid);

    }
  }

  // Check for sections.
  if (isset($import['sections'])) {
    foreach ($import['sections'] as $target => $data) {

      list($name,$original_id) = explode(':', $target);

      // This function will need to be generic enough to add a new batch whenever
      // we need to for: sections, section content, etc.
      oa_export_add_batch_set($name, $data, $original_id, $cloned_node->nid);

    }
  }

  // Prepare the space for import.
//  $new_space = oa_clone($original_space, $parent_space, NULL, TRUE, $import); // Try without last TRUE

  // Set a message.
  $context['message'] = t('Preparing to import Space: !title', array('!title' => $cloned_node->title));

  $context['results']['messages'][] = t('The Space: !title was imported.', array('!title' => $cloned_node->title));

  // Bump the progress indicator.
  $context['sandbox']['progress']++;

  // We are finished with a single space.
  $context['finished'] = $context['sandbox']['progress'] / $context['sandbox']['max'];
}

function oa_export_batch_import_comments_test($target, $data, $original_id, $parent_id, &$context) {

  // Array to hold child comments if we find any in this first iteration.
  $comment_children = array();

  $all_comments = array();

  foreach ($target as $key => $comment) {
    list($name,$original_id) = explode(':', $key);

    // Import all comments that don't have a parent comment... they are
    // 'top-level' comments.
    if (empty($comment->pid)) {

      // This is where it can get nuts. This is easy if the paragraph item is
      // paragraph_media or paragraph_text but...if it's paragraph_content or
      // paragraph_snippet then we have to go find the content we need in our
      // export and import it before we can relate it to the comment. What happens
      // when that content has paragraph items and they have paragraph_content?
      // This can get into recursion hell.

      if (isset($comment->field_oa_related) && !empty($comment->field_oa_related)) {

      }

      // Comment cid before saving.
      $original_cid = $comment->cid;

      // Prepare the comment for import.
      oa_export_import_prepare_comment($comment, $parent_id);
      comment_save($comment);

      // Comment cid after saving a new comment.
      $new_cid = $comment->cid;

      // Add cids to an array for later.
      $all_comments[$original_cid] = $new_cid;
      // We want to create a new comment.
//              oa_export_import_comment($comment, $old_section_content, $new_section_content->nid, $import_sections);
    }
    else {
      // Build an array of comments that need a parent created first.
      $comment_children[] = $comment;
    }
    /** @todo Figure out a better way to do this. */
    // This seems redundant but we must get all parent comments
    // imported first and then import child comments so they can be
    // associated with their parent.
    foreach ($comment_children as $comment) {
      // If there is a pid then this is a child comment.
      if (!empty($comment->pid)) {

        // Comment cid before saving.
        $original_cid = $comment->cid;

        // Prepare the comment for import.
        oa_export_import_prepare_comment($comment, $parent_id);
        comment_save($comment);

        // Comment cid after saving a new comment.
        $new_cid = $comment->cid;

        // Add cids to an array for later.
        $all_comments[$original_cid] = $new_cid;
        // We want to create a new comment.
//              oa_export_import_comment($comment, $old_section_content, $new_section_content->nid, $import_sections, $comment->pid);
      }
    }
  }


}

function oa_export_batch_import_section_test($target, $data, $original_id, $parent_id, &$context) {

  // Create a new section node associating it with the correct space.
  $cloned_node = oa_clone_prepare($target, $parent_id, NULL, TRUE, TRUE);

  // Save the new cloned (clean) node.
  unset($cloned_node->clone_from_original_nid);
  node_save($cloned_node);

  $import['current_section'] = $cloned_node->nid;

  $_SESSION['oa_export']['current_section'] = $cloned_node->nid;

  if (isset($data['paragraphs'])) {
    foreach ($import['paragraphs'] as $target => $data) {

      list($name,$original_id) = explode(':', $target);

      $para = array($name => $data);
      // This function will need to be generic enough to add a new batch whenever
      // we need to for: sections, section content, etc. Also need to change keys
      // for paragraphs on the export. We should key them the same as we do for
      // space:ID or section:ID.
      oa_export_add_batch_set($name, $para, $original_id, $cloned_node->nid);

    }
  }

  $parent = node_load($parent_id);

  // Set a message.
  $context['message'] = t('Preparing to import section: !title', array('!title' => $cloned_node->title));

  $context['results']['messages'][] = t('Section: !title was imported.', array('!title' => $cloned_node->title));

  // We are finished with a single space.
  $context['finished'] = TRUE;
}

function oa_export_batch_import_section_content_test(&$import, $content, &$context) {

  // Check to see if we have a piece of content that belongs to this section.
  if (!empty($original_section['section_content'])) {
    foreach ($original_section['section_content'] as $old_section_content_nid => $old_section_content) {
      // Create the new content node. We need the original node, the space nid,
      // and the section nid that relates to this piece of content.
      $new_section_content = oa_clone($old_section_content['content'], $new_space_nid, $new_section->nid, TRUE, TRUE);

      // TEMPORARY
      $import_sections = &$import['sections'];

      // Check for comments that are associated with this piece of content.
      if (isset($old_section_content['comments'])) {
        $comments = $old_section_content['comments'];
        // Array to hold child comments if we find any in this first iteration.
        $comment_children = array();

        $all_comments = array();

        foreach ($comments as $cid => $comment) {
          // Import all comments that don't have a parent comment.
          if (empty($comment->pid)) {

            // Comment cid before saving.
            $before = $comment->cid;

            // Prepare the comment for import.
            oa_export_import_prepare_comment($comment, $old_section_content_nid);
            comment_save($comment);

            // Comment cid after saving a new comment.
            $after = $comment->cid;

            // Add cids to an array for later.
            $all_comments[$before] = $after;
            // We want to create a new comment.
//              oa_export_import_comment($comment, $old_section_content, $new_section_content->nid, $import_sections);
          }
          else {
            // Build an array of comments that need a parent created first.
            $comment_children[] = $comment;
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
            oa_export_import_prepare_comment($comment, $old_section_content_nid);
            comment_save($comment);

            // Comment cid after saving a new comment.
            $after = $comment->cid;

            // Add cids to an array for later.
            $all_comments[$before] = $after;
            // We want to create a new comment.
//              oa_export_import_comment($comment, $old_section_content, $new_section_content->nid, $import_sections, $comment->pid);
          }
        }

        // Now update the comments we saved with any paragraph items they have.
        oa_export_import_comment_paragraph_items($all_comments, $new_section_content->nid, $old_section_content['paragraphs'], $import);

      }
      // Check the media field for files.
      if (!empty($new_section_content->field_oa_media)) {
        // We use this to find files related to the content.
        $wrapper = entity_metadata_wrapper('node', $new_section_content);
        oa_export_import_field_oa_media($wrapper);
      }
      // If $paragraph_comment exists then we know we are importing a section
      // with section content that is needed for a comment that has a
      // reference (paragraph_content) to a piece of content that hasn't been
      // imported yet.
      if (isset($paragraph_comment, $paragraph_comment->oa_export_target)) {
        // We know we are on the piece of content needed for our comment we
        // are trying to import.
        if ($old_section_content_nid == $paragraph_comment->oa_export_target['old_related_content_id']) {
          // This is the new nid of the content we are importing for the comment. Was 3 now 8.
          $paragraph_comment->oa_export_target['new_related_content_id'] = $new_section_content->nid;
        }
        // Call this again now that the content has been imported.
        oa_export_import_comment($paragraph_comment, $old_section_content, $old_section_content_nid, $import_sections);
        unset($paragraph_comment);
      }
    }
  }
}

function oa_export_import_batch_finished($success, $results, $operations) {

  if ($success && isset($results['messages'])) {
    foreach ($results['messages'] as $message) {
      drupal_set_message($message, 'status');
    }
  }
}


// ========================================================

/**
 * This imports node data once a node has been saved.
 * This can include: comments, paragraphs, etc.
 *
 * @param array $import
 * @param array $context
 * @param $imported_node
 *  The new node that was just imported
 * @param $original_node
 *  The original node we exported before cloning it.
 * @throws Exception
 */
function oa_export_import_related_data($import, &$context, $imported_node, $original_node) {

  // We need to check for files, comments, paragraphs in comments, paragraphs
  switch ($original_node->type) {
    case 'oa_space':
      break;
    case 'oa_section':
      $import = $import['sections']['section:' . $original_node->nid];
      break;
    case 'oa_group':
      break;
    default:
      break;
  }

  // Check for comments.
  if (isset($import['comment']) && !empty($import['comment']['comments'])) {
    foreach ($import['comment']['comments'] as $target => $data) {

      list($name,$original_id) = explode(':', $target);

      // This function will need to be generic enough to add a new batch whenever
      // we need to for: sections, section content, etc.
      oa_export_add_batch_set($name, $data, $original_id, $imported_node->nid);

    }
  }

  /** We need to import comments for paragraphs as well. */

  // Check for exported comments.
  if (isset($import['comment']) && !empty($import['comment']['comments'])) {

    // Import the comments for the saved node.
    oa_export_import_save_comments($import['comment']['comments'], $imported_node->nid);
  }


  /** We need to import sections. */


  /** We need to import paragraphs. */

  // Fields on the original node.
  $fields = field_info_instances('node', $original_node->type);

  // Iterate over the fields to find paragraphs fields.
  foreach ($fields as $field_name => $field) {

    // Right now field_oa_related is the only field we using for paragraphs so
    // we could save some cycles here by hard coding that field but this is
    // better if we ever add any other paragraphs fields.
    $info = field_info_field($field['field_name']);

    // This will find fields like field_oa_related that are 'paragraphs' type
    // and make sure it exists in the node.
    if ($info['type'] == 'paragraphs' && isset($imported_node->$field_name)) {
      // We use the original node here because paragraphs_clone_items()
      // unsets the field we need (field_oa_related).
      $language = $original_node->language;

      // Iterate over field_oa_related items and pass them individually to one
      // of the helper functions that either clones it instantly or stores it
      // for later if the node that is needed has been imported already.
      foreach ($original_node->{$field_name}[$language] as $delta => $item) {
        $paragraphs = $import['paragraphs'];

        // Trying something...

        // Still need a way to make this more generic. What
        // happens if someone creates a new paragraph_item entity. Right now
        // this is hardcoded for the ones we create by default. This seems to me
        // it may be better to do a query here that looks for the content (how
        // do we know what content to look for exactly), sees if it exists and
        // then calls one of the functions below. (clone) if it does and (store)
        // if it doesn't. Then each time a new batch operation runs for a section
        // or section_content we check to see if there are stored values, look
        // through those stored values to see if there is a paragraph stored for
        // it (based on the old nid of that section or section_content. For
        // example: If the $paragraphs[$item['value']]['bundle'] is either,
        // paragraph_content OR paragraph_snippet, then we need to check to see
        // if the 'content' or 'snippet' for the paragraph has been imported yet.
        // If it has then we can clone immediately otherwise we will need to store
        // it until the proper node has been imported.

        // We can import these right away.
        if ($paragraphs[$item['value']]['bundle'] == 'paragraph_media' || $paragraphs[$item['value']]['bundle'] == 'paragraph_text') {
          oa_export_clone_import_paragraph_item($import, $context, $paragraphs[$item['value']], 'node', $imported_node, $field_name, $language);
        }

        // Need to write a query here that will check to see if the master content
        // that the paragraph relates to has been imported yet. Not sure how we
        // will know what the new nid is of the content we would need to search
        // for is though?? Should be able to just do a node_load($new_nid) to check.

        // These may have to be imported later.
        if ($paragraphs[$item['value']]['bundle'] == 'paragraph_content' || $paragraphs[$item['value']]['bundle'] == 'paragraph_snippet') {
          oa_export_store_import_paragraph_item($import, $context, $paragraphs, 'node', $imported_node, $field_name, $language);
        }

        // end


//        oa_export_clone_paragraphs_items($import, $context, $paragraphs, 'node', $imported_node, $field_name, $language);
      }
    }
  }

  // We need to save the node again with the new data that has been added.
  node_save($imported_node);

}
