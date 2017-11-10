--TEST--
Read AMF3 basic parent child
--DESCRIPTION--
Reads a basic parent child in AMF3 format.



--SKIPIF--
<?php if (!extension_loaded('amf')) print 'skip'; ?>
--FILE--
<?php
include 'amf_decoder.inc';
$decoder = new AmfDecoder();
$data = unserialize(file_get_contents(__DIR__ . '/asset/value/basic-parent-child.amf3'));
$decoder->setData($data);
$obj = $decoder->run();
var_dump($obj);
?>
--EXPECTF--
object(stdClass)#%d (2) {
  ["parent"]=>
  NULL
  ["child"]=>
  object(stdClass)#%d (2) {
    ["parent"]=>
    *RECURSION*
    ["child"]=>
    NULL
  }
}
