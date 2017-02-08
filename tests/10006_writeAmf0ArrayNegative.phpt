--TEST--
Write AMF0 negative key Array 
--DESCRIPTION--
Writes a negative key Array in AMF0 format.



--SKIPIF--
<?php if (!extension_loaded('amf')) print 'skip'; ?>
--FILE--
<?php
include 'amf_encoder.inc';
$encoder = new AmfEncoder();
$encoder->setAvmPlus(false);
$data = unserialize(file_get_contents(__DIR__ . '/asset/value/array-negative.amf0'));
$obj = array();
$obj[-1] = 'a';
$obj[0] = 'b';
$obj[1] = 'c';
$res = $encoder->run($obj);
echo ($res === $data) ? 'same' : 'diff';
?>
--EXPECT--
same
