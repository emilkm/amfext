--TEST--
Write AMF0 typed Object 
--DESCRIPTION--
Writes a typed Object in AMF0 format.



--SKIPIF--
<?php if (!extension_loaded('amf')) print 'skip'; ?>
--FILE--
<?php
include __DIR__ . '/asset/value/VoExplicitTypeNotBlank.php';
include 'amf_encoder.inc';
$encoder = new AmfEncoder();
$encoder->setAvmPlus(false);
$data = unserialize(file_get_contents(__DIR__ . '/asset/value/object-typed-explicit-from-field.amf0'));
$obj = new \emilkm\tests\asset\value\VoExplicitTypeNotBlank();
$res = $encoder->run($obj);
echo ($res === $data) ? 'same' : 'diff';
?>
--EXPECT--
same
