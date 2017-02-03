--TEST--
Read AMF3 Boolean
--DESCRIPTION--
Reads a boolean in AMF3 format.



--SKIPIF--
<?php if (!extension_loaded('amf')) print 'skip'; ?>
--FILE--
<?php
include 'amf_decoder.inc';
$decoder = new AmfDecoder();
$data = unserialize(file_get_contents(__DIR__ . '/asset/value/boolean-true.amf3'));
$decoder->setData($data);
$obj = $decoder->run();
var_dump($obj->value);
$data = unserialize(file_get_contents(__DIR__ . '/asset/value/boolean-false.amf3'));
$decoder->setData($data);
$obj = $decoder->run();
var_dump($obj->value);
?>
--EXPECT--
bool(true)
bool(false)
