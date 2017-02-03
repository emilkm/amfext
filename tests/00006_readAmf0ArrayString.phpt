--TEST--
Read AMF0 string key Array
--DESCRIPTION--
Reads a string key (associative) array in AMF0 format.



--SKIPIF--
<?php if (!extension_loaded('amf')) print 'skip'; ?>
--FILE--
<?php
include 'amf_decoder.inc';
$decoder = new AmfDecoder();
$data = unserialize(file_get_contents(__DIR__ . '/asset/value/array-string.amf0'));
$decoder->setData($data);
$obj = $decoder->run();
var_dump($obj);
?>
--EXPECTF--
object(stdClass)#%d (3) {
  ["a"]=>
  float(1)
  ["b"]=>
  float(2)
  ["c"]=>
  float(3)
}
