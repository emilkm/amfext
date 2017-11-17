--TEST--
Write AMF3 Vector <Int>
--DESCRIPTION--
Writes a Vector of int in AMF3 format.



--SKIPIF--
<?php if (!extension_loaded('amf')) print 'skip'; ?>
--FILE--
<?php
include 'amf_encoder.inc';
$encoder = new AmfEncoder();
$encoder->setAvmPlus(true);
$data = unserialize(file_get_contents(__DIR__ . '/asset/value/vector-int.amf3'));
$obj = new stdClass();
$vector = new Vector(Vector::AMF3_VECTOR_INT, array(1, 2, 3));
$obj->value = $vector;
$res = $encoder->run($obj);
echo ($res === $data) ? 'same' : 'diff';
$data = unserialize(file_get_contents(__DIR__ . '/asset/value/vector-int-negative.amf3'));
$obj = new stdClass();
$vector = new Vector(Vector::AMF3_VECTOR_INT, array(-3, -2, -1));
$obj->value = $vector;
$res = $encoder->run($obj);
echo ($res === $data) ? 'same' : 'diff';
?>
--EXPECT--
samesame
