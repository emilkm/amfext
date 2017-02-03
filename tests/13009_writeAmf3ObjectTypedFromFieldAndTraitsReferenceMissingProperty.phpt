--TEST--
Write AMF3 typed Object 
--DESCRIPTION--
Writes a typed Object in AMF3 format.



--SKIPIF--
<?php if (!extension_loaded('amf')) print 'skip'; ?>
--FILE--
<?php
include 'amf_encoder.inc';
$encoder = new AmfEncoder();
$encoder->setAvmPlus(true);
$data = unserialize(file_get_contents(__DIR__ . '/asset/value/object-typed-lightclass-and-traits-reference-missing-property.amf3'));
$remoteClassField = AmfEncoder::REMOTE_CLASS_FIELD;
$v1 = new stdClass();
$v1->$remoteClassField = 'LightClass';
$v1->id = 1;
$v1->name = 'a';
$v2 = new stdClass();
$v2->$remoteClassField = 'LightClass';
$v2->id = 2;
//$v2->name = 'b'; //v2 is missing one of its properties
$obj = new stdClass();
$obj->value = array($v1, $v2);
$res = $encoder->run($obj);
echo ($res === $data) ? 'same' : 'diff';
?>
--EXPECT--
same
