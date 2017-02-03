--TEST--
Read AMF0 sparse Array
--DESCRIPTION--
Reads a sparse array in AMF0 format.



--SKIPIF--
<?php if (!extension_loaded('amf')) print 'skip'; ?>
--FILE--
<?php
include 'amf_decoder.inc';
$decoder = new AmfDecoder();
$data = unserialize(file_get_contents(__DIR__ . '/asset/value/array-sparse.amf0'));
$decoder->setData($data);
$obj = $decoder->run();
print_r($obj);
?>
--EXPECT--
Array
(
    [0] => a
    [2] => b
    [4] => c
)
