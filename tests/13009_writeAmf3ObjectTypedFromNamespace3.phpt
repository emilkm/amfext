--TEST--
Write AMF3 typed Object
--DESCRIPTION--
Writes a typed Object in AMF3 format.



--SKIPIF--
<?php if (!extension_loaded('amf')) print 'skip'; ?>
--FILE--
<?php
include __DIR__ . '/asset/value/VoTest.php';
include 'amf_encoder.inc';
$encoder = new AmfEncoder();
$data = unserialize(file_get_contents(__DIR__ . '/asset/value/object-typed-from-namespace3.amf3'));
$obj = new \emilkm\tests\asset\value\VoTest();
$obj->id = 1;
$obj->name = 'a';
$obj->children = null;
$res = $encoder->run($obj);
echo ($res === $data) ? 'same' : 'diff';
?>
--EXPECT--
same
