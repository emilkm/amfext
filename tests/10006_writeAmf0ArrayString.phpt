--TEST--
Write AMF0 string key Array 
--DESCRIPTION--
Writes a string key Array in AMF0 format.



--SKIPIF--
<?php if (!extension_loaded('amf')) print 'skip'; ?>
--FILE--
<?php
include 'amf_encoder.inc';
$encoder = new AmfEncoder();
$encoder->setAvmPlus(false);
$data = unserialize(file_get_contents(__DIR__ . '/asset/value/array-string.amf0'));
$obj = array();
$obj['a'] = 1;
$obj['b'] = 2;
$obj['c'] = 3;
$res = $encoder->run($obj);
echo ($res === $data) ? 'same' : 'diff';
?>
--EXPECT--
same
