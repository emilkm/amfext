--TEST--
Read AMF0 mixed key Array
--DESCRIPTION--
Reads a mixed key array in AMF0 format.



--SKIPIF--
<?php if (!extension_loaded('amf')) print 'skip'; ?>
--FILE--
<?php
include 'amf_decoder.inc';
$decoder = new AmfDecoder();
$data = unserialize(file_get_contents(__DIR__ . '/asset/value/array-mixed.amf0'));
$decoder->setData($data);
$obj = $decoder->run();
var_dump($obj);
?>
--EXPECT--
array(2) {
  [0]=>
  string(1) "a"
  ["b"]=>
  float(2)
}
