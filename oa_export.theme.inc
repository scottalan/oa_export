<?php

function oa_export_preprocess_table(&$vars) {
  if ($_GET['q'] == 'admin/structure/taxonomy/space_type') {
    $rows = &$vars['rows'];
    foreach ($rows as $key => &$row) {
      $parts = explode(':', $key);
      $tid = $parts[1];
      $row['data'][2] .= ' ' . l(t('export'), 'blueprint/export/' . $tid);
    }
  }
}
