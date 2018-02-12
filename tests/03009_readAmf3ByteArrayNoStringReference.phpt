--TEST--
Read AMF3 ByteArray and no String reference
--DESCRIPTION--
Reads a two byte arrays with identical strings (no string reference) in AMF3 format.



--SKIPIF--
<?php if (!extension_loaded('amf')) print 'skip'; ?>
--FILE--
<?php
include 'amf_decoder.inc';
$decoder = new AmfDecoder();
$data = unserialize(file_get_contents(__DIR__ . '/asset/value/bytearray-no-string-reference.amf3'));
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
