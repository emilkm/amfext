--TEST--
Read AMF3 ByteArray
--DESCRIPTION--
Reads a ByteArray in AMF3 format.



--SKIPIF--
<?php if (!extension_loaded('amf')) print 'skip'; ?>
--FILE--
<?php
include 'amf_decoder.inc';
$decoder = new AmfDecoder();
$data = unserialize(file_get_contents(__DIR__ . '/asset/value/bytearray.amf3'));
$decoder->setData($data);
$obj = $decoder->run();
var_dump($obj->value);
?>
--EXPECTF--
object(ByteArray)#%d (1) {
  ["data"]=>
  string(6) "1a2b3c"
}
