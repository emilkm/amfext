--TEST--
Read AMF3 Unicode String
--DESCRIPTION--
Reads a Unicode string in AMF3 format.



--SKIPIF--
<?php if (!extension_loaded('amf')) print 'skip'; ?>
--FILE--
<?php
include 'amf_decoder.inc';
$decoder = new AmfDecoder();
$data = unserialize(file_get_contents(__DIR__ . '/asset/value/string-unicode.amf3'));
$decoder->setData($data);
$obj = $decoder->run();
var_dump($obj->value);
?>
--EXPECT--
string(12) "витоша"
