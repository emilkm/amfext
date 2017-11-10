--TEST--
Decode packet typed object single
--DESCRIPTION--
Decode an AMF3 packet containing a single typed object in the request body.



--SKIPIF--
<?php if (!extension_loaded('amf')) print 'skip'; ?>
--FILE--
<?php
include __DIR__ . '/asset/value/VoTest.php';
include 'amf_decoder.inc';
$decoder = new AmfDecoder();
$data = unserialize(file_get_contents(__DIR__ . '/asset/request/packet-typed-object-single.amf3'));
$obj = $decoder->readMessage($data);
$tmp = $obj->bodies[0]->responseURI;
var_dump($obj);
?>
--EXPECTF--
object(ActionMessage)#%d (3) {
  ["version"]=>
  int(3)
  ["headers"]=>
  NULL
  ["bodies"]=>
  array(1) {
    [0]=>
    object(MessageBody)#%d (3) {
      ["targetURI"]=>
      string(4) "null"
      ["responseURI"]=>
      string(3) "/10"
      ["data"]=>
      object(flex\messaging\messages\RemotingMessage)#%d (9) {
        ["source"]=>
        string(7) "service"
        ["operation"]=>
        string(6) "method"
        ["messageId"]=>
        string(36) "63FCE70D-F447-ED49-83E6-00001695D4AF"
        ["clientId"]=>
        string(36) "F9F98C89-5099-E7BF-7997-9B41FC79A0D4"
        ["timestamp"]=>
        float(1437179933687)
        ["timeToLive"]=>
        int(0)
        ["destination"]=>
        string(6) "efxphp"
        ["headers"]=>
        object(stdClass)#5 (1) {
          ["DSId"]=>
          string(36) "F9F98C89-5099-E7BF-7997-9B41FC79A0D4"
        }
        ["body"]=>
        array(1) {
          [0]=>
          object(emilkm\tests\asset\value\VoTest)#%d (4) {
            ["id"]=>
            int(1)
            ["name"]=>
            string(1) "a"
            ["parent"]=>
            NULL
            ["children"]=>
            NULL
          }
        }
      }
    }
  }
}
