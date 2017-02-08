--TEST--
Write AMF0 typed Object to stdClass
--DESCRIPTION--
Writes a typed Object to stdClass in AMF0 format.
When the type cannot be resolved, simply write stdClass and set the _explicitType field.


--SKIPIF--
<?php if (!extension_loaded('amf')) print 'skip'; ?>
--FILE--
<?php
include 'amf_encoder.inc';
$encoder = new AmfEncoder();
$encoder->setAvmPlus(false);
$data = unserialize(file_get_contents(__DIR__ . '/asset/value/object-typed-someclass.amf0'));
$obj = new stdClass();
$remoteClassField = AmfEncoder::REMOTE_CLASS_FIELD;
$obj->$remoteClassField = 'SomeClass';
$res = $encoder->run($obj);
echo ($res === $data) ? 'same' : 'diff';
?>
--EXPECT--
same
