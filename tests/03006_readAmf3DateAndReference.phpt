--TEST--
Read AMF3 Date and Date reference 
--DESCRIPTION--
Reads a date, and a reference to date, in AMF3 format.



--SKIPIF--
<?php if (!extension_loaded('amf')) print 'skip'; ?>
--FILE--
<?php
include 'amf_decoder.inc';
$decoder = new AmfDecoder();
$data = unserialize(file_get_contents(__DIR__ . '/asset/value/date-and-reference.amf3'));
$decoder->setData($data);
$decoder->setUseInternalDateType(true);
$obj = $decoder->run();
var_dump($obj->value1);
var_dump($obj->value2);
$decoder->pos = 0;
$decoder->setUseInternalDateType(false);
$obj = $decoder->run();
var_dump($obj->value1);
var_dump($obj->value2);
?>
--EXPECTF--
object(Date)#%d (2) {
  ["timestamp"]=>
  float(1422995025)
  ["milli"]=>
  float(123)
}
object(Date)#%d (2) {
  ["timestamp"]=>
  float(1422995025)
  ["milli"]=>
  float(123)
}
object(DateTime)#%d (3) {
  ["date"]=>
  string(26) "2015-02-03 20:23:45.123000"
  ["timezone_type"]=>
  int(3)
  ["timezone"]=>
  string(3) "UTC"
}
object(DateTime)#%d (3) {
  ["date"]=>
  string(26) "2015-02-03 20:23:45.123000"
  ["timezone_type"]=>
  int(3)
  ["timezone"]=>
  string(3) "UTC"
}