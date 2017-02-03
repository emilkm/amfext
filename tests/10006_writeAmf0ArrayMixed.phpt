--TEST--
Write AMF0 mixed key Array 
--DESCRIPTION--
Writes a mixed key Array in AMF0 format.



--SKIPIF--
<?php if (!extension_loaded('amf')) print 'skip'; ?>
--FILE--
<?php
include 'amf_encoder.inc';
$encoder = new AmfEncoder();
$encoder->setAvmPlus(false);
$data = unserialize(file_get_contents(__DIR__ . '/asset/value/array-mixed.amf0'));
$obj = array();
$obj[0] = 'a';
$obj['b'] = 2;
$res = $encoder->run($obj);
echo ($res === $data) ? 'same' : 'diff';
?>
--EXPECT--
same
