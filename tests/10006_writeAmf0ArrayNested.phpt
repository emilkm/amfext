--TEST--
Write AMF0 nested Array 
--DESCRIPTION--
Writes a nested Array in AMF0 format.



--SKIPIF--
<?php if (!extension_loaded('amf')) print 'skip'; ?>
--FILE--
<?php
include 'amf_encoder.inc';
$encoder = new AmfEncoder();
$encoder->setAvmPlus(false);
$data = unserialize(file_get_contents(__DIR__ . '/asset/value/array-nested.amf0'));
$obj = array();
$obj['items'] = array();
$obj['items'][0] = 'a';
$obj['items'][1] = 'b';
$obj['items'][2] = 'c';
$res = $encoder->run($obj);
echo ($res === $data) ? 'same' : 'diff';
?>
--EXPECT--
same
