<?php

  $hwpins = array(
    array('pin'=>1, 'type'=>8), // 1: Motion detector (io=1)
    array('pin'=>2, 'type'=>4), // 2: Photoresist (io=2)
    array('pin'=>3, 'type'=>3), // 3: Relay
    array('pin'=>4, 'type'=>9), // 4: Voltage Ctrl
    array('pin'=>5, 'type'=>2), // 5: Buzzer
    array('pin'=>6, 'type'=>6)  // 6: DHT22 (io=3)
  );

  $hwtypes = array(
    array('id'=>1, 'sort'=>1, 'io'=>0, 'name'=>'LED',                'sname'=>'LED',      'descr'=>'Simple LED, 3.3 V', 'action'=>'Duty cycle', 'params'=>array('i:duty:milliseconds','i:cycle:milliseconds') ),
    array('id'=>2, 'sort'=>2, 'io'=>0, 'name'=>'Buzzer',             'sname'=>'Buzzer',   'descr'=>'Simple buzzer, probably a doorbell ;)', 'action'=>'Duty cycle', 'params'=>array('i:duty:milliseconds','i:cycle:milliseconds') ),
    array('id'=>3, 'sort'=>3, 'io'=>0, 'name'=>'Relay',              'sname'=>'Relay',    'descr'=>'High voltage solid-state or electromechanical relay', 'action'=>'State', 'params'=>array('s:off|on') ),
    array('id'=>4, 'sort'=>4, 'io'=>2, 'name'=>'Photoresistor',      'sname'=>'LDR',      'descr'=>'Measure light intensity', 'units'=> array(array('txt'=>'R, Ohm')) ),
    array('id'=>5, 'sort'=>5, 'io'=>3, 'name'=>'DS18B20 (T&deg;)',   'sname'=>'DS18B20',  'descr'=>'Measure temperature', 'units'=>array(array('txt'=>'T, &deg;C')) ),
    array('id'=>6, 'sort'=>6, 'io'=>3, 'name'=>'DHT22 (T&deg;+hum.)','sname'=>'DHT22',    'descr'=>'Measure temperature and humidity', 'units'=>array(array('txt'=>'T, &deg;C'),array('txt'=>'Humidity, %')) ),
    array('id'=>7, 'sort'=>7, 'io'=>0, 'name'=>'SG-90 servo',        'sname'=>'SG-90',    'descr'=>'Use to move or rotate small things', 'action'=>'Rotate', 'params'=>array('i:-90..90:degrees') ),
    array('id'=>8, 'sort'=>8, 'io'=>1, 'name'=>'Motion detector',    'sname'=>'MotionD',  'descr'=>'Apply rotation', 'action'=>'Rotate', 'params'=>array('s:off|on','i:+1/-1:direction') ),
    array('id'=>9, 'sort'=>9, 'io'=>0, 'name'=>'Voltage controller', 'sname'=>'VoltC',    'descr'=>'Adjustable voltage controller', 'action'=>'Voltage', 'params'=>array('i:0..220:volts') )
  );

  $sysactions = array(
    array('id'=>0, 'sort'=>1, 'action'=>'None',               'name'=>'None',   'params'=>array()),
    array('id'=>1, 'sort'=>7, 'action'=>'Followup',           'name'=>'System', 'params'=>array('i:delay:seconds','i:rule code')),
    array('id'=>2, 'sort'=>2, 'action'=>'Set sleep period',   'name'=>'System', 'params'=>array('i:sleep for #:seconds')),
    array('id'=>3, 'sort'=>3, 'action'=>'Set measure period', 'name'=>'System', 'params'=>array('i:every #:seconds')),
    array('id'=>4, 'sort'=>4, 'action'=>'Cloud connection',   'name'=>'System', 'params'=>array('s:off|periodic|constant','i:number of:measures')),
    array('id'=>5, 'sort'=>5, 'action'=>'Cloud sync',         'name'=>'System', 'params'=>array('i:sync every #:measures')),
    array('id'=>6, 'sort'=>6, 'action'=>'Notification',       'name'=>'System', 'params'=>array('t:notification message'))
  );

  $rules = array(
    array(
      'code'=>'LON',
      'name'=>'Motion activated light',
      'type'=>0,
      'conditions'=>array(
        array('id'=>11, 'ival'=>array(array('1',''))),      // motion
        array('id'=>12, 'ival'=>array(array("50","999"))),   // light
        array('id'=>0,  'ival'=>array(array("1:0:0:0","7:23:59:59"))) // period
      ),
      'actions'=>array(
        array('id'=>13, 'parm'=>array("1")),
        array('id'=>14, 'parm'=>array("220")),
        array('id'=>6,  'parm'=>array("Light is on")),
        array('id'=>1,  'parm'=>array("300","LOFF"))
      )
    ),
    array(
      'code'=>'LOFF',
      'name'=>'Light off',
      'type'=>1,
      'conditions'=>array(),
      'actions'=>array(
        array('id'=>13, 'parm'=>array("0")),
        array('id'=>14, 'parm'=>array("0"))
      )
    ),
    array(
      'code'=>'ALM1',
      'name'=>'Alarm on',
      'type'=>0,
      'conditions'=>array(
        array('id'=>1, 'ival'=>array(array("0 7 1-5 1-12 *"))) // cron
      ),
      'actions'=>array(
        array('id'=>15, 'parm'=>array("20","50")),
        array('id'=>1,  'parm'=>array("120","ALM2"))
      )
    ),
    array(
      'code'=>'ALM2',
      'name'=>'Alarm off',
      'type'=>1,
      'conditions'=>array(),
      'actions'=>array(
        array('id'=>15, 'parm'=>array("0","0"))
      )
    )
  );
?>