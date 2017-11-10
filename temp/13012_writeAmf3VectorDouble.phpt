--TEST--
Write AMF3 Vector <Double>
--DESCRIPTION--
Writes a Vector of double in AMF3 format.



--SKIPIF--
<?php if (!extension_loaded('amf')) print 'skip'; ?>
--FILE--
<?php
include 'amf_encoder.inc';
$encoder = new AmfEncoder();
$encoder->setAvmPlus(true);
$data = unserialize(file_get_contents(__DIR__ . '/asset/value/vector-double.amf3'));
$obj = new stdClass();
$vector = new Vector(Vector::AMF3_VECTOR_DOUBLE, array(-31.57, 0, 31.57));
$obj->value = $vector;
$res = $encoder->run($obj);
echo ($res === $data) ? 'same' : 'diff';
?>
--EXPECT--
same
