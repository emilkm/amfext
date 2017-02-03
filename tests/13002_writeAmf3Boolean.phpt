--TEST--
Write AMF3 Boolean 
--DESCRIPTION--
Writes a Boolean in AMF3 format.



--SKIPIF--
<?php if (!extension_loaded('amf')) print 'skip'; ?>
--FILE--
<?php
include 'amf_encoder.inc';
$encoder = new AmfEncoder();
$encoder->setAvmPlus(true);
$data = unserialize(file_get_contents(__DIR__ . '/asset/value/boolean-false.amf3'));
$obj = new stdClass();
$obj->value = false;
$res = $encoder->run($obj);
echo ($res === $data) ? 'same' : 'diff';
$data = unserialize(file_get_contents(__DIR__ . '/asset/value/boolean-true.amf3'));
$encoder->setAvmPlus(true);
$obj = new stdClass();
$obj->value = true;
$res = $encoder->run($obj);
echo ($res === $data) ? 'same' : 'diff';
?>
--EXPECT--
samesame
