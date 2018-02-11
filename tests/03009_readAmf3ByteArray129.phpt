--TEST--
Read AMF3 ByteArray containing a string greater than 128 bytes
--DESCRIPTION--
Reads a ByteArray, containing a string greater than 128 bytes, in AMF3 format.



--SKIPIF--
<?php if (!extension_loaded('amf')) print 'skip'; ?>
--FILE--
<?php
include 'amf_decoder.inc';
$decoder = new AmfDecoder();
$data = unserialize(file_get_contents(__DIR__ . '/asset/value/bytearray129.amf3'));
$decoder->setData($data);
$obj = $decoder->run();
var_dump($obj->value);
?>
--EXPECTF--
object(ByteArray)#%d (1) {
  ["data"]=>
  string(129) "111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111"
}
