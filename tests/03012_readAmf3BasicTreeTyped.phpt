--TEST--
Read AMF3 basic tree typed
--DESCRIPTION--
Reads a basic tree typed in AMF3 format.



--SKIPIF--
<?php if (!extension_loaded('amf')) print 'skip'; ?>
--FILE--
<?php
include 'amf_decoder.inc';
$decoder = new AmfDecoder();
$data = unserialize(file_get_contents(__DIR__ . '/asset/value/basic-tree-typed.amf3'));
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
  ["children"]=>
  array(1) {
    [0]=>
    object(stdClass)#%d (3) {
      ["_explicitType"]=>
      string(9) "SomeClass"
      ["parent"]=>
      *RECURSION*
      ["children"]=>
      NULL
    }
  }
}
