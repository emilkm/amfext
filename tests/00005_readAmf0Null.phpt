--TEST--
Read AMF0 Null
--DESCRIPTION--
Reads a null in AMF0 format.



--SKIPIF--
<?php if (!extension_loaded('amf')) print 'skip'; ?>
--FILE--
<?php
include 'amf_decoder.inc';
$decoder = new AmfDecoder();
$data = unserialize(file_get_contents(__DIR__ . '/asset/value/null.amf0'));
$decoder->setData($data);
$obj = $decoder->run();
var_dump($obj->value);
?>
--EXPECT--
NULL
