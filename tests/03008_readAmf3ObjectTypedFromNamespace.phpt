--TEST--
Read AMF3 typed Object
--DESCRIPTION--
Reads a typed object in AMF3 format. Type comes from namespace.



--SKIPIF--
<?php if (!extension_loaded('amf')) print 'skip'; ?>
--FILE--
<?php
include __DIR__ . '/asset/value/VoExplicitTypeNotSet.php';
include 'amf_decoder.inc';
$decoder = new AmfDecoder();
$data = unserialize(file_get_contents(__DIR__ . '/asset/value/object-typed-explicit-from-namespace.amf3'));
$decoder->setData($data);
$obj = $decoder->run();
var_dump($obj);
?>
--EXPECTF--
object(emilkm\tests\asset\value\VoExplicitTypeNotSet)#%d (0) {
}
