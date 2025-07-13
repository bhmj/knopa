<?php

  require_once('i_hwtype.php');

sleep(1);

  if (isset($_REQUEST['chupd'])) {
    $f = fopen("knopa_log.log", 'a+');
    fwrite($f,"Version query\n\n");
    fclose($f);
    die('{"new_ver":"0.5b", "size":"218"}');
  }

  if (isset($_REQUEST['doupd'])) {
    $f = fopen("knopa_log.log", 'a+');
    fwrite($f,"Firmware update\n\n");
    fclose($f);
    die('{"result":"ok"}');
  }

  if (isset($_REQUEST['status'])) {
    $f = fopen("knopa_log.log", 'a+');
//    fwrite($f,"STATUS REQ\n\n");
//    fclose($f);
    die('{"apstatus":"connected to Cerberus"}');
  }

  if (isset($_REQUEST['wifi'])) {
    $ssid = $_REQUEST['wifissid'];
    $pass = $_REQUEST['wifipass'];
    $bssid = $_REQUEST['wifibssid'];
    $f = fopen("knopa_log.log", 'a+');
    fwrite($f,"WIFI CONFIG:\nSSID = $ssid\nPassword = $pass\nBSSID=$bssid\n\n");
    fclose($f);
    die('{"result":"ok"}');
  }

  if (isset($_REQUEST['sys'])) {
    $apssid  = $_REQUEST['apssid'];
    $appass  = $_REQUEST['appass'];
    $name    = $_REQUEST['name'];
    $measure = $_REQUEST['measure'];
    $pwsave  = $_REQUEST['pwsave'];
    $autoupd = $_REQUEST['autoupd'];
    $f = fopen("knopa_log.log", 'a+');
    fwrite($f,"SYS CONFIG:\nAP SSID = $apssid\nAP password = $appass\nDevice name = $name\nMeasure period = $measure (sec)\nPower save = $pwsave\nAuto update = $autoupd\n\n");
    fclose($f);
    die('{"result":"ok"}');
  }
  if (isset($_REQUEST['cloud'])) {
    $serv1 = $_REQUEST['clserv1'];
    $user1 = $_REQUEST['cluser1'];
    $pass1 = $_REQUEST['clpass1'];
    $serv2 = $_REQUEST['clserv2'];
    $user2 = $_REQUEST['cluser2'];
    $pass2 = $_REQUEST['clpass2'];
    $conn  = $_REQUEST['clconn'];
    $sync  = $_REQUEST['clsync'];
    $f = fopen("knopa_log.log", 'a+');
    fwrite($f,"CLOUD CONFIG:\nServer1 = $serv1\nUser = $user1\nPassword = $pass1\nServer2 = $serv2\nUser = $user2\nPassword = $pass2\nConnection type = $conn\nSync rounds = $sync\n\n");
    fclose($f);
    die('{"result":"ok"}');
  }

  if (isset($_REQUEST['ntp'])) {
    $ntps1 = $_REQUEST['ntps1'];
    $ntps2 = $_REQUEST['ntps2'];
    $ntps3 = $_REQUEST['ntps3'];
    $tz   = $_REQUEST['tz'];
    $f = fopen("knopa_log.log", 'a+');
    fwrite($f,"TIME CONFIG:\nServer1 = $ntps1\nServer2 = $ntps2\nServer3 = $ntps3\nTimezone = $tz\n\n");
    fclose($f);
    die('{"result":"ok"}');
  }

  if (isset($_REQUEST['dtm'])) {
    $str = $_REQUEST['str'];
    $f = fopen("knopa_log.log", 'a+');
    fwrite($f,"TIME SETUP:\nDate/time = $str\n\n");
    fclose($f);
    die('{"result":"ok"}');
  }

  if (isset($_REQUEST['sync'])) {
    sleep(3);
    $f = fopen("knopa_log.log", 'a+');
    fwrite($f,"NTP query\n\n");
    fclose($f);
    die('{"ctime":"1461021233200"}');
  }

  if (isset($_REQUEST['reboot'])) {
    $f = fopen("knopa_log.log", 'a+');
    fwrite($f,"REBOOT\n\n");
    fclose($f);
    die();
  }

  if (isset($_REQUEST['scan'])) {
    $f = fopen("knopa_log.log", 'a+');
    fwrite($f,"SCAN FOR APs\n\n");
    fclose($f);
    die('[{"ssid":"HomeRouter","ch":6,"rssi":"12.6","sec":"WPA2"},{"ssid":"Loner","ch":1,"rssi":"6.2","sec":"OPEN"}]');
  }

  if (isset($_REQUEST['conf'])) {
    die();
  }

/// "ctime":"1461021233200",
  die('{"devinfo":"80Mhz, 8Mbit (512+512)",'.
      ' "fver":"0.33a",'.
      ' "sdkboot":"1.5.2 + 5",'.
      ' "memtot":"65536",'.
      ' "memfree":"43226",'.
      ' "uptime":"131696",'.
      ' "apssid":"HomeRouter",'.
      ' "appass":"12345678",'.
      ' "name":"MyDevice",'.
      ' "pwsave":1,'.
      ' "measure":600,'.
      ' "ntps1":"0.europe.pool.ntp.org",'.
      ' "ntps2":"1.europe.pool.ntp.org",'.
      ' "ntps3":"2.europe.pool.ntp.org",'.
      ' "tz":180,'.
      ' "apbssid":"",'.
      ' "clserv1":"https://knopa.online",'.
      ' "cluser1":"username",'.
      ' "clpass1":"password",'.
      ' "clserv2":"http://my.server",'.
      ' "cluser2":"user",'.
      ' "clpass2":"pass",'.
      ' "clconn":2,'.
      ' "clsync":3,'.
      ' "hwtype":'.json_encode($hwtype).', '.
      ' "pin":[{"id":1,"type":1}]'.
      '}'
  );
?>