<?php

  $hwpins = array(
    array('pin'=>1, 'type'=>'1'),  // LED
    array('pin'=>2, 'type'=>'6'),  // Relay
    array('pin'=>5, 'type'=>'7'),  // Photoresistor
    array('pin'=>7, 'type'=>'10'), // Servo
    array('pin'=>8, 'type'=>'9')   // DS18B20
  );

  $hwtype = array(
    array('id'=>1, 'sort'=>1, 'io'=>0, 'name'=>'LED',                'descr'=>'Simple LED, 3.3 V', 'action'=>'Duty cycle', 'params'=>'i:msec,i:msec' ),
    array('id'=>2, 'sort'=>2, 'io'=>0, 'name'=>'RGB LED (R)',        'descr'=>'Red line',   'action'=>'Duty cycle', 'params'=>'i:msec,i:msec' ),
    array('id'=>3, 'sort'=>3, 'io'=>0, 'name'=>'RGB LED (G)',        'descr'=>'Green line', 'action'=>'Duty cycle', 'params'=>'i:msec,i:msec' ),
    array('id'=>4, 'sort'=>4, 'io'=>0, 'name'=>'RGB LED (B)',        'descr'=>'Blue line',  'action'=>'Duty cycle', 'params'=>'i:msec,i:msec' ),
    array('id'=>5, 'sort'=>5, 'io'=>0, 'name'=>'Buzzer',             'descr'=>'Simple buzzer, probably a doorbell ;)', 'action'=>'Duty cycle', 'params'=>'i:msec,i:msec' ),
    array('id'=>6, 'sort'=>6, 'io'=>0, 'name'=>'Relay',              'descr'=>'High voltage solid-state or electromechanical relay', 'action'=>'State', 'params'=>'b:on|off' ),
    array('id'=>7, 'sort'=>7, 'io'=>1, 'name'=>'Photoresistor',      'descr'=>'Measure light intensity' ),
    array('id'=>8, 'sort'=>8, 'io'=>1, 'name'=>'DS18B20 (T&deg;)',   'descr'=>'Measure temperatures from -55&deg;C to +125&deg;C with &plusmn;0.5&deg;C accuracy from -10&deg;C to +85&deg;C' ),
    array('id'=>9, 'sort'=>9, 'io'=>1, 'name'=>'DHT22 (T&deg;+hum.)','descr'=>'Measure temperature and humidity' ),
    array('id'=>10,'sort'=>10,'io'=>0, 'name'=>'SG-90 servo',        'descr'=>'Use to move or rotate small things', 'action'=>'Rotate', 'params'=>'i:degrees' ),
    array('id'=>11,'sort'=>11,'io'=>0, 'name'=>'Motor',              'descr'=>'Apply rotation', 'action'=>'Rotate', 'params'=>'b:on|off,i:direction' ),
    array('id'=>12,'sort'=>12,'io'=>0, 'name'=>'IR Controller',      'descr'=>'Control IR devices like TV or AC', 'action'=>'Command', 'params'=>'s:code' ),
    array('id'=>13,'sort'=>13,'io'=>0, 'name'=>'Voltage controller', 'descr'=>'Adjustable voltage controller', 'action'=>'Voltage', 'params'=>'b:on|off,i:volts')
  );

  $acts = array(
    array('id'=>1, 'sort'=>1, 'action'=>'Sleep setup', 'params'=>'i:sec'),
    array('id'=>2, 'sort'=>2, 'action'=>'Measure setup', 'params'=>'i:sec'),
    array('id'=>3, 'sort'=>3, 'action'=>'Cloud connection', 'params'=>'i:off|sync|constant'),
    array('id'=>4, 'sort'=>4, 'action'=>'Cloud sync', 'params'=>'i:measures'),
    array('id'=>5, 'sort'=>5, 'action'=>'Notification', 'params'=>'s:message')
  );
?>