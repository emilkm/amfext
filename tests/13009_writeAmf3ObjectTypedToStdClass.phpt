--TEST--
Write AMF3 typed Object to stdClass
--DESCRIPTION--
Writes a typed Object to stdClass in AMF3 format.
When the type cannot be resolved, simply write stdClass and set the _explicitType field.


--SKIPIF--
<?php if (!extension_loaded('amf')) print 'skip'; ?>
--FILE--
<?php
include 'amf_encoder.inc';
$encoder = new AmfEncoder();
$encoder->setAvmPlus(true);
$data = unserialize(file_get_contents(__DIR__ . '/asset/value/object-typed-someclass.amf3'));
$obj = new stdClass();
$remoteClassField = AmfEncoder::REMOTE_CLASS_FIELD;
$obj->$remoteClassField = 'SomeClass';
$res = $encoder->run($obj);
echo ($res === $data) ? 'same' : 'diff';
?>
--EXPECT--
same
