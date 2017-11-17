--TEST--
Read AMF0 Date
--DESCRIPTION--
Reads a date in AMF0 format.



--SKIPIF--
<?php if (!extension_loaded('amf')) print 'skip'; ?>
--FILE--
<?php
include 'amf_decoder.inc';
$decoder = new AmfDecoder();
$data = unserialize(file_get_contents(__DIR__ . '/asset/value/date.amf0'));
$decoder->setData($data);
$decoder->setUseRlandDateType(true);
$obj = $decoder->run();
var_dump($obj->value);
$decoder->pos = 0;
$decoder->setUseRlandDateType(false);
$obj = $decoder->run();
var_dump($obj->value);
?>
--EXPECTF--
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
