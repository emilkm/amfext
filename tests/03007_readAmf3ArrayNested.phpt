--TEST--
Read AMF3 nested Array
--DESCRIPTION--
Reads a nested array in AMF3 format, as associative array or object.


--SKIPIF--
<?php if (!extension_loaded('amf')) print 'skip'; ?>
--FILE--
<?php
include 'amf_decoder.inc';
$decoder = new AmfDecoder();
$data = unserialize(file_get_contents(__DIR__ . '/asset/value/array-nested-as-assoc.amf3'));
$decoder->setData($data);
$obj = $decoder->run();
var_dump($obj);
$data = unserialize(file_get_contents(__DIR__ . '/asset/value/array-nested-as-object.amf3'));
$decoder->setData($data);
$obj = $decoder->run();
var_dump($obj);
?>
--EXPECTF--
array(1) {
  ["items"]=>
  array(3) {
    [0]=>
    string(1) "a"
    [1]=>
    string(1) "b"
    [2]=>
    string(1) "c"
  }
}
object(stdClass)#%d (1) {
  ["items"]=>
  array(3) {
    [0]=>
    string(1) "a"
    [1]=>
    string(1) "b"
    [2]=>
    string(1) "c"
  }
}
