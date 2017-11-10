--TEST--
Read AMF3 Vector <Uint>
--DESCRIPTION--
Reads a Vecotor of uint in AMF3 format.



--SKIPIF--
<?php if (!extension_loaded('amf')) print 'skip'; ?>
--FILE--
<?php
include 'amf_decoder.inc';
$decoder = new AmfDecoder();
$data = unserialize(file_get_contents(__DIR__ . '/asset/value/vector-uint.amf3'));
$decoder->setData($data);
$obj = $decoder->run();
var_dump($obj);
?>
--EXPECTF--
object(stdClass)#%d (1) {
  ["value"]=>
  object(Vector)#%d (3) {
    ["type"]=>
    int(14)
    ["fixed"]=>
    bool(false)
    ["data"]=>
    array(3) {
      [0]=>
      int(2147483647)
      [1]=>
      float(2147483648)
      [2]=>
      float(4294967295)
    }
  }
}
