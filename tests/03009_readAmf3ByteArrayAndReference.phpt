--TEST--
Read AMF3 ByteArray and ByteArray reference
--DESCRIPTION--
Reads a ByteArray and a reference to a ByteArray in AMF3 format.



--SKIPIF--
<?php if (!extension_loaded('amf')) print 'skip'; ?>
--FILE--
<?php
include 'amf_decoder.inc';
$decoder = new AmfDecoder();
$data = unserialize(file_get_contents(__DIR__ . '/asset/value/bytearray-and-reference.amf3'));
$decoder->setData($data);
$obj = $decoder->run();
var_dump($obj->value1);
var_dump($obj->value2);
?>
--EXPECTF--
object(ByteArray)#%d (1) {
  ["data"]=>
  string(6) "1a2b3c"
}
object(ByteArray)#%d (1) {
  ["data"]=>
  string(6) "1a2b3c"
}
