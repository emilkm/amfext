--TEST--
Read AMF3 Vector <Object> and Vector reference
--DESCRIPTION--
Reads a Vector of object, and a reference to Vector, in AMF3 format.



--SKIPIF--
<?php if (!extension_loaded('amf')) print 'skip'; ?>
--FILE--
<?php
include 'amf_decoder.inc';
$decoder = new AmfDecoder();
$data = unserialize(file_get_contents(__DIR__ . '/asset/value/vector-object-and-reference.amf3'));
$decoder->setData($data);
$obj = $decoder->run();
var_dump($obj);
?>
--EXPECTF--
object(stdClass)#%d (2) {
  ["value1"]=>
  object(Vector)#%d (3) {
    ["type"]=>
    int(16)
    ["fixed"]=>
    bool(false)
    ["data"]=>
    array(3) {
      [0]=>
      object(stdClass)#%d (1) {
        ["value"]=>
        int(1)
      }
      [1]=>
      object(stdClass)#%d (1) {
        ["value"]=>
        int(2)
      }
      [2]=>
      object(stdClass)#%d (1) {
        ["value"]=>
        int(3)
      }
    }
  }
  ["value2"]=>
  object(Vector)#3 (3) {
    ["type"]=>
    int(16)
    ["fixed"]=>
    bool(false)
    ["data"]=>
    array(3) {
      [0]=>
      object(stdClass)#%d (1) {
        ["value"]=>
        int(1)
      }
      [1]=>
      object(stdClass)#%d (1) {
        ["value"]=>
        int(2)
      }
      [2]=>
      object(stdClass)#%d (1) {
        ["value"]=>
        int(3)
      }
    }
  }
}
