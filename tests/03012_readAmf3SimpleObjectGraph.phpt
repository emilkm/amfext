--TEST--
Read AMF3 simple Object graph
--DESCRIPTION--
Reads a simple object graph in AMF3 format.



--SKIPIF--
<?php if (!extension_loaded('amf')) print 'skip'; ?>
--FILE--
<?php
include 'amf_decoder.inc';
$decoder = new AmfDecoder();
$data = unserialize(file_get_contents(__DIR__ . '/asset/value/simple-object-graph.amf3'));
$decoder->setData($data);
$obj = $decoder->run();
var_dump($obj);
?>
--EXPECTF--
object(stdClass)#%d (1) {
  ["value"]=>
  array(1) {
    [0]=>
    object(stdClass)#%d (3) {
      ["name"]=>
      string(1) "a"
      ["parent"]=>
      NULL
      ["children"]=>
      array(1) {
        [0]=>
        object(stdClass)#%d (3) {
          ["name"]=>
          string(2) "a1"
          ["parent"]=>
          *RECURSION*
          ["children"]=>
          NULL
        }
      }
    }
  }
}
