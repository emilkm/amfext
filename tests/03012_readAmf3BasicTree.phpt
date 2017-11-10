--TEST--
Read AMF3 basic tree
--DESCRIPTION--
Reads a basic tree in AMF3 format.



--SKIPIF--
<?php if (!extension_loaded('amf')) print 'skip'; ?>
--FILE--
<?php
include 'amf_decoder.inc';
$decoder = new AmfDecoder();
$data = unserialize(file_get_contents(__DIR__ . '/asset/value/basic-tree.amf3'));
$decoder->setData($data);
$obj = $decoder->run();
var_dump($obj);
?>
--EXPECTF--
object(stdClass)#%d (2) {
  ["parent"]=>
  NULL
  ["children"]=>
  array(1) {
    [0]=>
    object(stdClass)#%d (2) {
      ["parent"]=>
      *RECURSION*
      ["children"]=>
      NULL
    }
  }
}
