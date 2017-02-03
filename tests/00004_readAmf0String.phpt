--TEST--
Read AMF0 String 
--DESCRIPTION--
Reads a string in AMF0 format.



--SKIPIF--
<?php if (!extension_loaded('amf')) print 'skip'; ?>
--FILE--
<?php
include 'amf_decoder.inc';
$decoder = new AmfDecoder();
$data = unserialize(file_get_contents(__DIR__ . '/asset/value/string.amf0'));
$decoder->setData($data);
$obj = $decoder->run();
var_dump($obj->value);
?>
--EXPECT--
string(3) "abc"
