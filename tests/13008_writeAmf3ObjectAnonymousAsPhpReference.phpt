--TEST--
Write AMF3 anonymous Object PHP reference
--DESCRIPTION--
Writes an anonymous Object PHP reference in AMF3 format.
--SKIPIF--
<?php if (!extension_loaded('amf')) print 'skip'; ?>
--FILE--
<?php
include 'amf_encoder.inc';
$encoder = new AmfEncoder();
$encoder->setAvmPlus(true);
$data = unserialize(file_get_contents(__DIR__ . '/asset/value/object-anonymous.amf3'));
//pure anonymous
$obj0 = new stdClass();
$obj = &$obj0;
$res = $encoder->run($obj);
echo ($res === $data) ? 'same' : 'diff';
?>
--EXPECT--
same
