--TEST--
Read AMF3 basic parent child typed
--DESCRIPTION--
Reads a basic parent child typed in AMF3 format.



--SKIPIF--
<?php if (!extension_loaded('amf')) print 'skip'; ?>
--FILE--
<?php
include 'amf_decoder.inc';
$decoder = new AmfDecoder();
$data = unserialize(file_get_contents(__DIR__ . '/asset/value/basic-parent-child-typed.amf3'));
$decoder->setData($data);
$obj = $decoder->run();
var_dump($obj);
?>
--EXPECTF--
object(stdClass)#%d (3) {
  ["_explicitType"]=>
  string(9) "SomeClass"
  ["parent"]=>
  NULL
  ["child"]=>
  object(stdClass)#%d (3) {
    ["_explicitType"]=>
    string(9) "SomeClass"
    ["parent"]=>
    *RECURSION*
    ["child"]=>
    NULL
  }
}
