--TEST--
Read AMF0 dense Array
--DESCRIPTION--
Reads a dense array in AMF0 format.



--SKIPIF--
<?php if (!extension_loaded('amf')) print 'skip'; ?>
--FILE--
<?php
include 'amf_decoder.inc';
$decoder = new AmfDecoder();
$data = unserialize(file_get_contents(__DIR__ . '/asset/value/array-dense.amf0'));
$decoder->setData($data);
$obj = $decoder->run();
print_r($obj);
?>
--EXPECT--
Array
(
    [0] => a
    [1] => b
    [2] => c
)
