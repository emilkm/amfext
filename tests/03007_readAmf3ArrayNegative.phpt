--TEST--
Read AMF3 negative key Array
--DESCRIPTION--
Reads a negative key array in AMF3 format, as associative array or object.



--SKIPIF--
<?php if (!extension_loaded('amf')) print 'skip'; ?>
--FILE--
<?php
include 'amf_decoder.inc';
$decoder = new AmfDecoder();
$data = unserialize(file_get_contents(__DIR__ . '/asset/value/array-negative-as-assoc.amf3'));
$decoder->setData($data);
$obj = $decoder->run();
var_dump($obj);
$data = unserialize(file_get_contents(__DIR__ . '/asset/value/array-negative-as-object.amf3'));
$decoder->setData($data);
$obj = $decoder->run();
var_dump($obj);
?>
--EXPECTF--
array(3) {
  [-1]=>
  string(1) "a"
  [0]=>
  string(1) "b"
  [1]=>
  string(1) "c"
}
object(stdClass)#%d (3) {
  ["-1"]=>
  string(1) "a"
  ["0"]=>
  string(1) "b"
  ["1"]=>
  string(1) "c"
}
