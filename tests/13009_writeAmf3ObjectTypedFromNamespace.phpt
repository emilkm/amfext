--TEST--
Write AMF3 typed Object 
--DESCRIPTION--
Writes a typed Object in AMF3 format.



--SKIPIF--
<?php if (!extension_loaded('amf')) print 'skip'; ?>
--FILE--
<?php
include __DIR__ . '/asset/class/VoExplicitTypeNotSet.php';
include 'amf_encoder.inc';
$encoder = new AmfEncoder();
$encoder->setAvmPlus(true);
$data = unserialize(file_get_contents(__DIR__ . '/asset/value/object-typed-explicit-from-namespace.amf3'));
$obj = new \emilkm\tests\asset\value\VoExplicitTypeNotSet();
$res = $encoder->run($obj);
echo ($res === $data) ? 'same' : 'diff';
?>
--EXPECT--
same
