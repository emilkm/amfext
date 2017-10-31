--TEST--
Write AMF3 action message.
--DESCRIPTION--
Writes an action message in AMF3 format.



--SKIPIF--
<?php if (!extension_loaded('amf')) print 'skip'; ?>
--FILE--
<?php
include __DIR__ . '/asset/value/VoTest.php';
include 'amf_encoder.inc';
$encoder = new AmfEncoder();
$encoder->setAvmPlus(true);
//$data = unserialize(file_get_contents(__DIR__ . '/asset/request/packet-typed-object-multiple.amf3'));

$votest1 = new emilkm\tests\asset\value\VoTest();
$votest1->id = 1;
$votest1->name = 'a';
$votest2 = new emilkm\tests\asset\value\VoTest();
$votest2->id = 2;
$votest2->name = 'b';
$operations = [];
$operation = [
    'source' => 'service',
    'operation' => 'method',
    'body' => [$votest1, $votest2]
];
$operations[] = $operation;

$actionMessage = $encoder->createRemotingActionMessage(3, 10, $operations);
$res = $encoder->writeMessage($actionMessage);
var_dump($actionMessage);
//file_put_contents(__DIR__ . '/asset/request/packet-typed-object-multiple.amf3', serialize($res));
//$encoder->curlSend($res, 'http://emil-iis/700/dev-artena-efxphp7/server/amf.php');
?>
--EXPECTF--
object(ActionMessage)#4 (3) {
  ["version"]=>
  int(3)
  ["headers"]=>
  NULL
  ["bodies"]=>
  array(1) {
    [0]=>
    object(MessageBody)#5 (4) {
      ["targetURI"]=>
      string(4) "null"
      ["responseURI"]=>
      string(3) "/10"
      ["data"]=>
      object(flex\messaging\messages\RemotingMessage)#6 (10) {
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
        object(stdClass)#7 (1) {
          ["DSId"]=>
          string(36) "F9F98C89-5099-E7BF-7997-9B41FC79A0D4"
        }
        ["body"]=>
        array(2) {
          [0]=>
          object(emilkm\tests\asset\value\VoTest)#2 (4) {
            ["id"]=>
            int(1)
            ["name"]=>
            string(1) "a"
            ["parent"]=>
            NULL
            ["children"]=>
            NULL
          }
          [1]=>
          object(emilkm\tests\asset\value\VoTest)#3 (4) {
            ["id"]=>
            int(2)
            ["name"]=>
            string(1) "b"
            ["parent"]=>
            NULL
            ["children"]=>
            NULL
          }
        }
        ["_explicitType"]=>
        string(39) "flex.messaging.messages.RemotingMessage"
      }
      ["_explicitType"]=>
      string(33) "flex.messaging.io.amf.MessageBody"
    }
  }
}

