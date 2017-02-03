--TEST--
Write AMF3 Vector <UInt>
--DESCRIPTION--
Writes a Vector of uint in AMF3 format.



--SKIPIF--
<?php if (!extension_loaded('amf')) print 'skip'; ?>
--FILE--
<?php
include 'amf_encoder.inc';
$encoder = new AmfEncoder();
$encoder->setAvmPlus(true);
$data = unserialize(file_get_contents(__DIR__ . '/asset/value/vector-uint.amf3'));
$obj = new stdClass();
$vector = new Vector(Vector::AMF3_VECTOR_UINT, array(2147483647, 2147483648, 4294967295));
$obj->value = $vector;
$res = $encoder->run($obj);
echo ($res === $data) ? 'same' : 'diff';
?>
--EXPECT--
same
