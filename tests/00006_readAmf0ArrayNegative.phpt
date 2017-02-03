--TEST--
Read AMF0 negative key Array
--DESCRIPTION--
Reads a negative key array in AMF0 format.



--SKIPIF--
<?php if (!extension_loaded('amf')) print 'skip'; ?>
--FILE--
<?php
include 'amf_decoder.inc';
$decoder = new AmfDecoder();
$data = unserialize(file_get_contents(__DIR__ . '/asset/value/array-negative.amf0'));
$decoder->setData($data);
$obj = $decoder->run();
var_dump($obj);
?>
--EXPECT--
array(3) {
  [-1]=>
  string(1) "a"
  [0]=>
  string(1) "b"
  [1]=>
  string(1) "c"
}
