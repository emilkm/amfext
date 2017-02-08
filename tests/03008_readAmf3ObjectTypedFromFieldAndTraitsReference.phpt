--TEST--
Read AMF3 typed Object
--DESCRIPTION--
Reads a typed object in AMF3 format.



--SKIPIF--
<?php if (!extension_loaded('amf')) print 'skip'; ?>
--FILE--
<?php
include 'amf_decoder.inc';
$decoder = new AmfDecoder();
$data = unserialize(file_get_contents(__DIR__ . '/asset/value/object-typed-lightclass-and-traits-reference.amf3'));
$decoder->setData($data);
$obj = $decoder->run();
var_dump($obj);
?>
--EXPECTF--
object(stdClass)#%d (1) {
  ["value"]=>
  array(2) {
    [0]=>
    object(stdClass)#%d (3) {
      ["_explicitType"]=>
      string(10) "LightClass"
      ["id"]=>
      int(1)
      ["name"]=>
      string(1) "a"
    }
    [1]=>
    object(stdClass)#%d (3) {
      ["_explicitType"]=>
      string(10) "LightClass"
      ["id"]=>
      int(2)
      ["name"]=>
      string(1) "b"
    }
  }
}
