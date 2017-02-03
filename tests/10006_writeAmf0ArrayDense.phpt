--TEST--
Write AMF0 dense Array 
--DESCRIPTION--
Writes a dense Array in AMF0 format.



--SKIPIF--
<?php if (!extension_loaded('amf')) print 'skip'; ?>
--FILE--
<?php
include 'amf_encoder.inc';
$encoder = new AmfEncoder();
$encoder->setAvmPlus(false);
$data = unserialize(file_get_contents(__DIR__ . '/asset/value/array-dense.amf0'));
$obj = array();
$obj[0] = 'a';
$obj[1] = 'b';
$obj[2] = 'c';
$res = $encoder->run($obj);
echo ($res === $data) ? 'same' : 'diff';
?>
--EXPECT--
same
