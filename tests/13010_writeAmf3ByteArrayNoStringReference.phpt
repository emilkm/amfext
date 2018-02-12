--TEST--
Write AMF3 ByteArray and no string reference
--DESCRIPTION--
Writes a ByteArray and no string reference in AMF3 format.



--SKIPIF--
<?php if (!extension_loaded('amf')) print 'skip'; ?>
--FILE--
<?php
include 'amf_encoder.inc';
$encoder = new AmfEncoder();
$encoder->setAvmPlus(true);
$data = unserialize(file_get_contents(__DIR__ . '/asset/value/bytearray-no-string-reference.amf3'));
$obj = new stdClass();
$obj->value1 = new ByteArray('1a2b3c');
$obj->value2 = new ByteArray('1a2b3c');
$res = $encoder->run($obj);
echo ($res === $data) ? 'same' : 'diff';
?>
--EXPECT--
same
