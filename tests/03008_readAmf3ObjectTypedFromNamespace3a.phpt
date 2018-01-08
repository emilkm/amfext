--TEST--
Read AMF3 typed Object
--DESCRIPTION--
Reads typed object in AMF3 format. Type comes from namespace.
Value object is available and we expect _explicitType not to be set.


--SKIPIF--
<?php if (!extension_loaded('amf')) print 'skip'; ?>
--FILE--
<?php
include __DIR__ . '/asset/value/VoTest.php';
include 'amf_decoder.inc';
$decoder = new AmfDecoder();
$data = unserialize(file_get_contents(__DIR__ . '/asset/value/object-typed-from-namespace3.amf3'));
$decoder->setData($data);
$obj = $decoder->run();
var_dump($obj);
?>
--EXPECTF--
object(emilkm\tests\asset\value\VoTest)#%d (4) {
  ["id"]=>
  float(1)
  ["name"]=>
  string(1) "a"
  ["parent"]=>
  NULL
  ["children"]=>
  NULL
}
