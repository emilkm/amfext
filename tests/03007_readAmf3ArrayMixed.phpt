--TEST--
Read AMF3 mixed key Array
--DESCRIPTION--
Reads a mixed key array in AMF3 format, as associative array or object.



--SKIPIF--
<?php if (!extension_loaded('amf')) print 'skip'; ?>
--FILE--
<?php
include 'amf_decoder.inc';
$decoder = new AmfDecoder();
$data = unserialize(file_get_contents(__DIR__ . '/asset/value/array-mixed-as-assoc.amf3'));
$decoder->setData($data);
$obj = $decoder->run();
var_dump($obj);
$data = unserialize(file_get_contents(__DIR__ . '/asset/value/array-mixed-as-object.amf3'));
$decoder->setData($data);
$obj = $decoder->run();
var_dump($obj);
?>
--EXPECTF--
array(2) {
  ["b"]=>
  int(2)
  [0]=>
  string(1) "a"
}
object(stdClass)#%d (2) {
  ["0"]=>
  string(1) "a"
  ["b"]=>
  int(2)
}
