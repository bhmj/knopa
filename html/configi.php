<?php
  require_once('i_hwtype.php');

sleep(0);

  if (isset($_REQUEST['refresh'])) {
    die('{"result":"ok"}');
  }

  if (isset($_REQUEST['rules'])) {
    $r = $_REQUEST['rules'];
    $f = fopen('knopa_log.log', 'a+');
    fwrite($f,"Config rules: $r\n\n");
    fclose($f);
  }

  if (isset($_REQUEST['read'])) {
    $pin = $_REQUEST['read'];
    $f = fopen('knopa_log.log', 'a+');
    fwrite($f,"Config query for pin $pin\n\n");
    fclose($f);

    foreach($hwtypes as $i=>$hw)
      if ($hw['io']==1) {
        $hwtypes[$i]['units'] = array(array('txt'=>'Logical level'));
      }

    $str = '{';

    $str .= '"name":""';

    $str .= ', "hwpins":'.json_encode($hwpins);

    $str .= ', "hwtypes":'.json_encode($hwtypes);

    $str .= ',"sysacts":'.json_encode($sysactions);

    $str .= ',"rules":'.json_encode($rules);

    $str .= '}';

    die($str);
  }

?>