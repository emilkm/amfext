--TEST--
Read AMF3 sparse Array
--DESCRIPTION--
Reads a sparse array in AMF3 format, as associative array or object.



--SKIPIF--
<?php if (!extension_loaded('amf')) print 'skip'; ?>
--FILE--
<?php
include 'amf_decoder.inc';
$decoder = new AmfDecoder();
$data = unserialize(file_get_contents(__DIR__ . '/asset/value/array-sparse-as-assoc.amf3'));
$decoder->setData($data);
$obj = $decoder->run();
var_dump($obj);
$data = unserialize(file_get_contents(__DIR__ . '/asset/value/array-sparse-as-object.amf3'));
$decoder->setData($data);
$obj = $decoder->run();
var_dump($obj);
?>
--EXPECT--
array(3) {
  [2]=>
  string(1) "b"
  [4]=>
  string(1) "c"
  [0]=>
  string(1) "a"
}
object(stdClass)#2 (3) {
  ["0"]=>
  string(1) "a"
  ["2"]=>
  string(1) "b"
  ["4"]=>
  string(1) "c"
}
