--TEST--
Write AMF3 mixed key Array 
--DESCRIPTION--
Writes a mixed key Array in AMF3 format.



--SKIPIF--
<?php if (!extension_loaded('amf')) print 'skip'; ?>
--FILE--
<?php
include 'amf_encoder.inc';
$encoder = new AmfEncoder();
$encoder->setAvmPlus(true);
$data = unserialize(file_get_contents(__DIR__ . '/asset/value/array-mixed-as-object.amf3'));
$encoder->encodeAmf3nsndArrayAsObject(true);
$obj = array();
$obj[0] = 'a';
$obj['b'] = 2;
$res = $encoder->run($obj);
echo ($res === $data) ? 'same' : 'diff';
$data = unserialize(file_get_contents(__DIR__ . '/asset/value/array-mixed-as-assoc.amf3'));
$encoder->setAvmPlus(true);
$encoder->encodeAmf3nsndArrayAsObject(false);
$obj = array();
$obj[0] = 'a';
$obj['b'] = 2;
$res = $encoder->run($obj);
echo ($res === $data) ? 'same' : 'diff';
?>
--EXPECT--
samesame
