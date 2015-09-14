<?php
/**
 * @file
 * Exports the blueprint to a module. When the module is enabled the blueprint will be imported.
 */

define('OA_EXPORT_DEFAULT_MODULE_PATH', 'sites/all/modules');

/**
 * Form used to export a blueprint to a module.
 *
 * @param array $form
 *   Empty form array.
 * @param object $blueprint
 *   The fully loaded blueprint entity.
 *
 * @return array $form
 *   The fully built form used to collect data to generate a module.
 */
function oa_export_generate_module_form($form, &$form_state, $blueprint) {

  $form['module']['blueprint'] = array(
    '#type' => 'hidden',
    '#value' => $blueprint->tid,
  );
  $form['module']['help'] = array(
    '#markup' => t('Creates a module that will import the blueprint when installed or adds the export to an existing module.'),
  );
  $form['module']['choose'] = array(
    '#title' => t('Choose an option.'),
    '#type' => 'select',
    '#options' => array('new' => t('Create a new module'), 'existing' => t('Add to an existing module')),
    '#default_value' => isset($form_state['input']['choose']) ? $form_state['input']['choose'] : 'new',
    '#ajax' => array(
      'callback' => 'oa_export_define_module_callback',
      'wrapper' => 'generate-module-form',
      'methos' => 'replace',
      'effect' => 'fade',
    ),
  );
  $form['generate_module_form'] = array(
    '#title' => '',
    '#prefix' => '<div id="generate-module-form">',
    '#suffix' => '</div>',
    '#type' => 'fieldset',
  );

  if ($form['module']['choose']['#default_value'] == 'new') {
    oa_export_generate_new_module_form($form, $form_state);
  }
  else if ($form['module']['choose']['#default_value'] == 'existing') {
    oa_export_generate_existing_module_form($form, $form_state);
  }

  return $form;
}

/**
 * Ajax callback for choosing the type of module export. The are either creating a new module or they want to add the
 * export to an existing module.
 *
 * @param $form
 *   An associative array containing the structure of the form.
 * @param $form_state
 *   A keyed array containing the current state of the form.
 *
 * @return array
 *   The form used to generate a module.
 */
function oa_export_define_module_callback($form, $form_state) {
  return $form['generate_module_form'];
}

/**
 * Handles validation for the 'generate_module' form.
 *
 * @param $form
 *   An associative array containing the structure of the form.
 * @param $form_state
 *   A keyed array containing the current state of the form.
 */
function oa_export_generate_module_form_validate($form, &$form_state) {
  // Seems like the best way to determine which form needs what validation.
  switch ($form_state['triggering_element']['#name']) {
    case 'choose':
      break;

    case 'new_module':
      oa_export_generate_new_module_validate($form, $form_state);
      break;

    case 'existing_module':
      oa_export_generate_existing_module_validate($form, $form_state);
      break;

    default:
      break;
  }
}

/**
 * Generates the form for creating a new module to add the export to.
 *
 * @param $form
 *   An associative array containing the structure of the form.
 * @param $form_state
 *   A keyed array containing the current state of the form.
 */
function oa_export_generate_new_module_form(&$form, &$form_state) {
  $dcc = DRUPAL_CORE_COMPATIBILITY;
  $form['generate_module_form']['name'] = array(
    '#title' => t('Name'),
    '#description' => t('Example: Project Blueprint') . ' (' . t('Do not begin name with numbers.') . ')',
    '#type' => 'textfield',
    '#required' => TRUE,
    '#default_value' => isset($form_state['input']['name']) ? $form_state['input']['name'] : '',
  );
  $form['generate_module_form']['machine_name'] = array(
    '#type' => 'machine_name',
    '#title' => t('Machine-readable name'),
    '#description' => t('Example: project_blueprint' .
      '<br/>' .
      t('May only contain lowercase letters, numbers and underscores. <strong>Try to avoid conflicts with the names of existing Drupal projects.</strong>')
    ),
    '#required' => TRUE,
    '#default_value' => isset($form_state['input']['machine_name']) ? $form_state['input']['machine_name'] : '',
    '#machine_name' => array(
      'exists' => 'oa_export_module_machine_name_validate',
      'source' => array('generate_module_form', 'name'),
    ),
  );
  $form['generate_module_form']['description'] = array(
    '#title' => t('Description'),
    '#description' => t('Provide a short description of what users should expect when they enable your module.'),
    '#type' => 'textfield',
    '#default_value' => isset($form_state['input']['description']) ? $form_state['input']['description'] : '',
  );
  $form['generate_module_form']['version'] = array(
    '#title' => t('Version'),
    '#description' => t('Examples: @examples', array('@examples' => $dcc . '-1.0, ' . $dcc . '-1.0-beta1')),
    '#type' => 'textfield',
    '#required' => FALSE,
    '#default_value' => isset($form_state['input']['version']) ? $form_state['input']['version'] : '',
    '#element_validate' => array('oa_export_module_version_validate'),
  );
  $form['generate_module_form']['package'] = array(
    '#title' => t('Package'),
    '#description' => t('The group it will be displayed under in the module list.'),
    '#type' => 'textfield',
    '#required' => FALSE,
    '#default_value' => isset($form_state['input']['package']) ? $form_state['input']['package'] : '',
  );
  $form['generate_module_form']['generate_path'] = array(
    '#title' => t('Path to generate module'),
    '#description' => t('The relative path where the module will be created.') .
      t('Leave blank for <strong>@path</strong>.', array('@path' => OA_EXPORT_DEFAULT_MODULE_PATH)),
    '#type' => 'textfield',
    '#required' => FALSE,
    '#default_value' => isset($form_state['input']['generate_path']) ? $form_state['input']['generate_path'] : '',
    '#element_validate' => array('oa_export_module_generate_path_validate'),
  );
  $form['generate_module_form']['generate'] = array(
    '#type' => 'submit',
    '#value' => t('Generate module'),
    '#name' => 'new_module',
  );
}

/**
 * Handles validation on the 'machine_name' form element.
 *
 * @param $element
 *   The form element being validated.
 * @param $form_state
 *   A keyed array containing the current state of the form.
 * @param $form
 *   An associative array containing the structure of the form.
 */
function oa_export_module_machine_name_validate($element, &$form_state, $form) {
  $modules = system_rebuild_module_data();
  foreach ($modules as $name => $module) {
    if ($element === $name) {
      form_error($element, t(
        'The machine name @name is already being used by another module. Please choose another.', array(
          '@name' => $name,
        )
      ));
    }
  }
}

/**
 * Handles validation on the 'version' form element.
 *
 * Similar to @see features_export_form_validate_field().
 *
 * @param $element
 *   The form element being validated.
 * @param $form_state
 *   A keyed array containing the current state of the form.
 * @param $form
 *   An associative array containing the structure of the form.
 */
function oa_export_module_version_validate($element, &$form_state, $form) {
  preg_match('/^(?P<core>\d+\.x)-(?P<major>\d+)\.(?P<patch>\d+)-?(?P<extra>\w+)?$/', $element['#value'], $matches);
  if (!empty($element['#value']) && !isset($matches['core'], $matches['major'])) {
    $example = DRUPAL_CORE_COMPATIBILITY . '-1.0';
    form_error($element, t('Please enter a valid version with core and major version number. Example: @example', array('@example' => $example)));
  }
}

/**
 * Handles validation on the 'generate_path' form element.
 *
 * @param $element
 *   The form element being validated.
 * @param $form_state
 *   A keyed array containing the current state of the form.
 * @param $form
 *   An associative array containing the structure of the form.
 */
function oa_export_module_generate_path_validate($element, &$form_state, $form) {
  if (!empty($element['#value'])) {
    $generate_path = DRUPAL_ROOT . '/' . $element['#value'];
    if (!is_dir($generate_path)) {
      form_error($element, t('Invalid path, make sure @path is a valid path and you have access to it.', array('@path' => $generate_path)));
    }
  }
}

/**
 * Generates the form for adding the export to an existing module. This one is much simpler. We just need
 * the name of the module we will be adding the export to.
 *
 * @param $form_state
 *   A keyed array containing the current state of the form.
 * @param $form
 *   An associative array containing the structure of the form.
 */
function oa_export_generate_existing_module_form(&$form, &$form_state) {
  $form['generate_module_form']['generate_path'] = array(
    '#title' => t('Name of module'),
    '#description' => t('This should be the <strong>machine_name</strong> of an existing module in your system'),
    '#type' => 'textfield',
    '#required' => TRUE,
    '#default_value' => isset($form_state['input']['generate_path']) ? $form_state['input']['generate_path'] : '',
    '#element_validate' => array('oa_export_existing_module_name_validate'),
  );
  $form['generate_module_form']['generate'] = array(
    '#type' => 'submit',
    '#value' => t('Add export to module'),
    '#name' => 'existing_module',
  );
}

/**
 * Handles validation on the 'generate_path' form element when it is used while exporting to an existing module.
 *
 * @param $element
 *   The form element being validated.
 * @param $form_state
 *   A keyed array containing the current state of the form.
 * @param $form
 *   An associative array containing the structure of the form.
 */
function oa_export_existing_module_name_validate($element, &$form_state, $form) {
  if (!empty($element['#value']) && !module_exists($element['#value'])) {
    form_error($element, t(
      'The @name module does not exist.', array(
        '@name' => $element['#value'],
      )
    ));
  }
}

/**
 * Handles validation on the 'new_module' form.
 *
 * @param $form
 *   An associative array containing the structure of the form.
 * @param $form_state
 *   A keyed array containing the current state of the form.
 */
function oa_export_generate_new_module_validate($form, &$form_state) {

  // We need to validate that we can create a directory based on the form input.
  $info = array(
    'name' => $form_state['values']['name'],
    'machine_name' => $form_state['values']['machine_name'],
    'description' => $form_state['values']['description'],
    'package' => $form_state['values']['package'],
    'core' => DRUPAL_CORE_COMPATIBILITY,
    'version' => $form_state['values']['version'],
  );

  // @todo: Try storing the info array in form state storage.
  $form_state['storage']['module_info'] = $info;

  // We will be creating a new module in sites/all/modules.
  if (empty($form_state['values']['generate_path'])) {
    $module_path = DRUPAL_ROOT . '/' . OA_EXPORT_DEFAULT_MODULE_PATH . '/' . $info['machine_name'];
  }
  // Either we are creating a new module at a different path or just adding the export to an existing module.
  else {
    // The path to where the module will be written as defined by the user.
    $generate_path = $form_state['values']['generate_path'];
    $module_path = DRUPAL_ROOT . '/' . $form_state['values']['generate_path'];
  }

  $_SESSION['oa_export'] = array();
  // Store the module path so we have access after the batch runs.
  // @todo: See if we can get rid of this by trying to call drupal_get_path('module', MODULE_NAME); in the batch finish function.
  $_SESSION['oa_export']['module_path'] = $module_path;
  $_SESSION['oa_export']['module'] = $info['machine_name'];
  $_SESSION['oa_export']['type'] = 'new';

  // We need to create directories and some files. Doing it here so if their creation isn't successful we can throw
  // some errors.
  if (!oa_export_create_directories($module_path . '/oa_export/files')) {
    form_set_error(NULL, t("Couldn't create the module: @module_name at the path @module_path. Please check your permissions.", array(
        '@module_name' => $info['machine_name'],
        '@module_path' => $module_path,
      )
    ));
  }
  else {
    // Set the directory for file export.
    $_SESSION['oa_export']['files_directory'] = $module_path . '/oa_export/files';
    // File types used to create a basic module.
    $file_types = array('module', 'info', 'install');
    foreach ($file_types as $extension) {
      $success = oa_export_create_module_file($module_path, $info, $extension);
      if (!$success) {
        // Remove the oa_export directory that was created.
        oa_export_cleanup($module_path);
        // Set a form error.
        form_set_error(NULL, t("Couldn't create the @module_name.@extension file at the path @module_path. Please check your permissions.", array(
            '@extension' => $extension,
            '@module_name' => $info['machine_name'],
            '@module_path' => $module_path,
          )
        ));
      }
    }
  }
}

/**
 * Handles validation on the 'existing_module' form.
 *
 * @param $form
 *   An associative array containing the structure of the form.
 * @param $form_state
 *   A keyed array containing the current state of the form.
 */
function oa_export_generate_existing_module_validate($form, &$form_state) {

  // Get the path to the existing module.
  $module_path = drupal_get_path('module', $form_state['values']['generate_path']);

  $_SESSION['oa_export'] = array();

  // Store the module path so we have access after the batch runs.
  // @todo: See if we can get rid of this by trying to call drupal_get_path('module', MODULE_NAME); in the batch finish function.
  $_SESSION['oa_export']['module_path'] = $module_path;
  $_SESSION['oa_export']['module'] = $form_state['values']['generate_path'];
  $_SESSION['oa_export']['type'] = 'existing';

  // Try to create the oa_export directory. Set an error if we are unsuccessful.
  if (!oa_export_create_directories($module_path . '/oa_export')) {
    form_set_error(NULL, t("Couldn't create the oa_export directory in @module_name at the path @module_path. Please check your permissions.", array(
        '@module_name' => $form_state['values']['generate_path'],
        '@module_path' => $module_path,
      )
    ));
  }
  else {
    // Try to create the files directory.
    if (!oa_export_create_directories($module_path . '/oa_export/files')) {
      // Remove the oa_export directory that was created.
      oa_export_cleanup($module_path . '/oa_export');
      form_set_error(NULL, t("Couldn't create a files directory in @module_name/oa_export at the path @module_path. Please check your permissions.", array(
          '@module_name' => $form_state['values']['generate_path'],
          '@module_path' => $module_path,
        )
      ));
    }
  }

}

/**
 * The submit handler for the module generation form. This is what kicks off the batch process for the export.
 *
 * @param $form
 *   An associative array containing the structure of the form.
 * @param $form_state
 *   A keyed array containing the current state of the form.
 */
function oa_export_generate_module_form_submit($form, &$form_state) {
  $current_path = current_path();
  $blueprint_tid = $form_state['values']['blueprint'];

  $blueprint = taxonomy_term_load($blueprint_tid);
  oa_export_batch_export($blueprint, 'module');
  // Redirect the user to the current path.
  batch_process($current_path);
}

/**
 * Helper function to create the actual MODULE.module file.
 *
 * @param string $module_path
 *   The full 'realpath' to where the module resides.
 * @param array $info
 *   Contains values needed to create the module keyed by keys used to create the MODULE.info file.
 * @param $extension
 *   The extension of the file being created, e.g. module, info, install.
 *
 * @return bool
 *   Whether the file was successfully created or not.
 */
function oa_export_create_module_file($module_path, $info, $extension) {
  $function = 'oa_export_file_content_' . $extension;
  return $function($module_path, $info);
}

function oa_export_file_content_module($module_path, $info) {
  $output = "<?php\n/**\n* @file\n* Drupal needs this blank file.\n*/\n";

  if (file_put_contents($module_path . '/' . $info['machine_name'] . '.module', $output, FILE_APPEND) === FALSE) {
    return FALSE;
  }
  else {
    return TRUE;
  }
}

/**
 * Helper function to create the actual MODULE.info file.
 *
 * @param string $module_path
 *   The full 'realpath' to where the module resides.
 * @param array $info
 *   Contains values needed to create the module keyed by keys used to create the MODULE.info file.
 *
 * @return bool
 *   Whether the file was successfully created or not.
 */
function oa_export_file_content_info($module_path, $info) {
  $keys = array('name', 'description', 'package', 'core', 'version');
  $output = '';
  foreach ($keys as $key) {
    if (!empty($info[$key])) {
      $output .= "$key = $info[$key]\n";
    }
  }
  // We only need a dependency on oa_export for now.
  $output .= "dependencies[] = oa_export";

  if (file_put_contents($module_path . '/' . $info['machine_name'] . '.info', $output, FILE_APPEND|FILE_TEXT) === FALSE) {
    return FALSE;
  }
  else {
    return TRUE;
  }
}

/**
 * Helper function to create the actual MODULE.install file.
 *
 * @param string $module_path
 *   The full 'realpath' to where the module resides.
 * @param array $info
 *   Contains values needed to create the module keyed by keys used to create the MODULE.info file.
 *
 * @return bool
 *   Whether the file was successfully created or not.
 */
function oa_export_file_content_install($module_path, $info) {
  $output = '';
  $output .= "<?php\n/**\n* @file\n* Install, update and uninstall functions.\n*/\n";
  $output .= "\n";
  $output .= "function {$info['machine_name']}_install() {\n";
  $output .= "  // This should be indented. For code.\n";
  $output .= "}\n";

  if (file_put_contents($module_path . '/' . $info['machine_name'] . '.install', $output, FILE_APPEND) === FALSE) {
    return FALSE;
  }
  else {
    return TRUE;
  }
}
