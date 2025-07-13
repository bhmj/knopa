<?php
  require_once('i_hwtype.php');

sleep(1);

  if (isset($_REQUEST['read'])) {
    $pin = $_REQUEST['read'];
    $f = fopen('knopa_log.log', 'a+');
    fwrite($f,"Config query for pin $pin\n\n");
    fclose($f);

    $str = '{';

    foreach($hwpins as $hw) {
      if ($hw['pin']==$pin) {
        $pintype = $hw['type'];
        foreach($hwtype as $hwt) {
          if ($hwt['id']==$pintype)
            $str .= '"descr":"'.$hwt['descr'].'"';
        }
      }
    }


    $str .= ',"blank":{';

    foreach($hwtype as $hw) {
      if ($hw['id']==$pintype) {
        // conditions ------------------
        if ($hw['io']==0) {
          // XU:
          $str .= '"conditions":["cron","period"]';
        } else {
          // SR:
          $str .= '"conditions":["on","off","rise","fall"]';
        }
        // actions ---------------------
        $str .= ',"actions":[';
        // 1) system
        $i = 0;
        foreach($acts as $a) {
          if ($i) $str .= ',';
          $str .= '{"type":0,"action":"'.$a['action'].'","params":"'.$a['params'].'"}';
          $i++;
        }
        // 2) pin-specific
        foreach($hwpins as $hw) {
        }
        $str .= ']';
      }
    }

    $str .= '}}';

    die($str);
  }

?>