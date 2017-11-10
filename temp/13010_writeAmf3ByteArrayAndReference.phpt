--TEST--
Write AMF3 ByteArray and reference
--DESCRIPTION--
Writes a ByteArray in AMF3 format.



--SKIPIF--
<?php if (!extension_loaded('amf')) print 'skip'; ?>
--FILE--
<?php
include 'amf_encoder.inc';
$encoder = new AmfEncoder();
$encoder->setAvmPlus(true);
$data = unserialize(file_get_contents(__DIR__ . '/asset/value/bytearray-and-reference.amf3'));
$obj = new stdClass();
$bytearr = new ByteArray('1a2b3c');
$obj->value1 = $bytearr;
$obj->value2 = $bytearr;
$res = $encoder->run($obj);
echo ($res === $data) ? 'same' : 'diff';
?>
--EXPECT--
same
