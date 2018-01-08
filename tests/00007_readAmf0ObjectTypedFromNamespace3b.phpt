--TEST--
Read AMF0 typed Object
--DESCRIPTION--
Reads typed object in AMF0 format. Type comes from namespace.
Value object is not available and we expect _explicitType to be set.


--SKIPIF--
<?php if (!extension_loaded('amf')) print 'skip'; ?>
--FILE--
<?php
//include __DIR__ . '/asset/value/VoTest.php';
include 'amf_decoder.inc';
$decoder = new AmfDecoder();
$data = unserialize(file_get_contents(__DIR__ . '/asset/value/object-typed-from-namespace3.amf0'));
$decoder->setData($data);
$obj = $decoder->run();
var_dump($obj);
?>
--EXPECTF--
object(stdClass)#%d (5) {
  ["_explicitType"]=>
  string(31) "emilkm.tests.asset.value.VoTest"
  ["id"]=>
  float(1)
  ["name"]=>
  string(1) "a"
  ["parent"]=>
  NULL
  ["children"]=>
  NULL
}
