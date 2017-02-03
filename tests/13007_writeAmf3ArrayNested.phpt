--TEST--
Write AMF3 nested Array 
--DESCRIPTION--
Writes a nested Array in AMF3 format.



--SKIPIF--
<?php if (!extension_loaded('amf')) print 'skip'; ?>
--FILE--
<?php
include 'amf_encoder.inc';
$encoder = new AmfEncoder();
$encoder->setAvmPlus(true);
$data = unserialize(file_get_contents(__DIR__ . '/asset/value/array-nested-as-object.amf3'));
$encoder->encodeAmf3nsndArrayAsObject(true);
$obj = array();
$obj['items'] = array();
$obj['items'][0] = 'a';
$obj['items'][1] = 'b';
$obj['items'][2] = 'c';
$res = $encoder->run($obj);
echo ($res === $data) ? 'same' : 'diff';
$data = unserialize(file_get_contents(__DIR__ . '/asset/value/array-nested-as-assoc.amf3'));
$encoder->setAvmPlus(true);
$encoder->encodeAmf3nsndArrayAsObject(false);
$obj = array();
$obj['items'] = array();
$obj['items'][0] = 'a';
$obj['items'][1] = 'b';
$obj['items'][2] = 'c';
$res = $encoder->run($obj);
echo ($res === $data) ? 'same' : 'diff';
?>
--EXPECT--
samesame
