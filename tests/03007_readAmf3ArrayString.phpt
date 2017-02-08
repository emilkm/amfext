--TEST--
Read AMF3 string key Array
--DESCRIPTION--
Reads a string key (associative) array in AMF3 format, as associative array or object.



--SKIPIF--
<?php if (!extension_loaded('amf')) print 'skip'; ?>
--FILE--
<?php
include 'amf_decoder.inc';
$decoder = new AmfDecoder();
$data = unserialize(file_get_contents(__DIR__ . '/asset/value/array-string-as-assoc.amf3'));
$decoder->setData($data);
$obj = $decoder->run();
var_dump($obj);
$data = unserialize(file_get_contents(__DIR__ . '/asset/value/array-string-as-object.amf3'));
$decoder->setData($data);
$obj = $decoder->run();
var_dump($obj);
?>
--EXPECTF--
array(3) {
  ["a"]=>
  int(1)
  ["b"]=>
  int(2)
  ["c"]=>
  int(3)
}
object(stdClass)#%d (3) {
  ["a"]=>
  int(1)
  ["b"]=>
  int(2)
  ["c"]=>
  int(3)
}
