--TEST--
Read AMF3 Vector <Double>
--DESCRIPTION--
Reads a Vector of double in AMF3 format.



--SKIPIF--
<?php if (!extension_loaded('amf')) print 'skip'; ?>
--FILE--
<?php
include 'amf_decoder.inc';
$decoder = new AmfDecoder();
$data = unserialize(file_get_contents(__DIR__ . '/asset/value/vector-double.amf3'));
$decoder->setData($data);
$obj = $decoder->run();
var_dump($obj);
?>
--EXPECTF--
object(stdClass)#%d (1) {
  ["value"]=>
  object(Vector)#%d (3) {
    ["type"]=>
    int(15)
    ["fixed"]=>
    bool(false)
    ["data"]=>
    array(3) {
      [0]=>
      float(-31.57)
      [1]=>
      float(0)
      [2]=>
      float(31.57)
    }
  }
}
