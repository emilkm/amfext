--TEST--
Write AMF3 Vector <Object>
--DESCRIPTION--
Writes a Vector of object in AMF3 format.



--SKIPIF--
<?php if (!extension_loaded('amf')) print 'skip'; ?>
--FILE--
<?php
include 'amf_encoder.inc';
$encoder = new AmfEncoder();
$encoder->setAvmPlus(true);
$data = unserialize(file_get_contents(__DIR__ . '/asset/value/vector-object.amf3'));
$v1 = new stdClass();
$v1->value = 1;
$v2 = new stdClass();
$v2->value = 2;
$v3 = new stdClass();
$v3->value = 3;
$obj = new stdClass();
$vector = new Vector(Vector::AMF3_VECTOR_OBJECT, array($v1, $v2, $v3));
$obj->value = $vector;
$res = $encoder->run($obj);
echo ($res === $data) ? 'same' : 'diff';
?>
--EXPECT--
same
