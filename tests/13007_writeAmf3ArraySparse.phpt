--TEST--
Write AMF3 sparse Array 
--DESCRIPTION--
Writes a sparse Array in AMF3 format.



--SKIPIF--
<?php if (!extension_loaded('amf')) print 'skip'; ?>
--FILE--
<?php
include 'amf_encoder.inc';
$encoder = new AmfEncoder();
$encoder->setAvmPlus(true);
$data = unserialize(file_get_contents(__DIR__ . '/asset/value/array-sparse-as-object.amf3'));
$encoder->encodeAmf3nsndArrayAsObject(true);
$obj = array();
$obj[0] = 'a';
$obj[2] = 'b';
$obj[4] = 'c';
$res = $encoder->run($obj);
echo ($res === $data) ? 'same' : 'diff';
$data = unserialize(file_get_contents(__DIR__ . '/asset/value/array-sparse-as-assoc.amf3'));
$encoder->setAvmPlus(true);
$encoder->encodeAmf3nsndArrayAsObject(false);
$obj = array();
$obj[0] = 'a';
$obj[2] = 'b';
$obj[4] = 'c';
$res = $encoder->run($obj);
echo ($res === $data) ? 'same' : 'diff';
?>
--EXPECT--
samesame
