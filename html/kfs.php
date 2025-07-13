<?php

  require_once('i_hwtype.php');

//sleep(1);

  if (isset($_REQUEST['read'])) {
    die('{"wifissid":"Cerberus","wifipass":"12345678",dir:[{fname:"kfs.html",fsize:7396},{fname:"knopa.m.css",fsize:3113}]}');
  }

ob_start();
phpinfo();
$info = ob_get_contents();
ob_end_clean();

$fp = fopen("phpinfo.html", "w+");
fwrite($fp, $info);
fclose($fp);

  if (isset($_FILES)) {
sleep(3);
    $r = $_FILES;
ob_start();
print_r($r);
$info = ob_get_contents();
ob_end_clean();
    $f = fopen('knopa_log.log', 'a+');
    fwrite($f,"Uploaded: $r\n\n");
    fclose($f);
  }

  if (isset($_REQUEST['dump'])) {

    $mfrom = isset($_REQUEST['mfrom']) ? $_REQUEST['mfrom'] : 0;
    $mlen  = isset($_REQUEST['mlen'])  ? $_REQUEST['mlen'] : 0;

    if ($mfrom) if (preg_match('/(0x)|(h)|[ABCDEF]/i',$mfrom)) $mfrom = hexdec($mfrom); else $mfrom = intval($mfrom);
    if ($mlen)  if (preg_match('/(0x)|(h)|[ABCDEF]/i',$mlen))  $mlen  = hexdec($mlen);  else $mlen  = intval($mlen);

    $mto   = $mfrom+$mlen-1;

    header('Content-Type: application/octet-stream');
    header('Content-Disposition: attachment; filename="dump'.strtoupper(str_pad(dechex($mfrom),5,'0',STR_PAD_LEFT)).'-'.strtoupper(str_pad(dechex($mto),5,'0',STR_PAD_LEFT)).'.bin"');

    echo("Hey there!");
  }

?>