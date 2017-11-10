--TEST--
Encode packet object with integer property name
--DESCRIPTION--
Encode an AMF3 packet an object with integer property name.



--SKIPIF--
<?php if (!extension_loaded('amf')) print 'skip'; ?>
--FILE--
<?php
include 'amf_encoder.inc';
$encoder = new AmfEncoder();
$encoder->setAvmPlus(true);
//$data = unserialize(file_get_contents(__DIR__ . '/asset/request/packet-typed-object-single.amf3'));

$intprop = 11;
$obj = new stdClass();
$obj->_explicitType = 'VoIntegerProperty';
$obj->id = 1;
$obj->name = 'a';
$obj->$intprop = 'aa';

$bodies = [];
$bodies[] = [$obj];

$actionMessage = $encoder->createAcknowledgeActionMessage(3, 10, $bodies);
$res = $encoder->writeMessage($actionMessage);
var_dump($actionMessage);
//file_put_contents(__DIR__ . '/asset/request/packet-typed-object-single.amf3', serialize($res));
//$encoder->curlSend($res, 'http://emil-iis/700/dev-artena-efxphp7/server/amf.php');
?>
--EXPECTF--
object(ActionMessage)#3 (3) {
  ["version"]=>
  int(3)
  ["headers"]=>
  NULL
  ["bodies"]=>
  array(1) {
    [0]=>
    object(MessageBody)#4 (4) {
      ["targetURI"]=>
      string(12) "/10/onResult"
      ["responseURI"]=>
      string(4) "null"
      ["data"]=>
      object(flex\messaging\messages\AcknowledgeMessage)#5 (8) {
        ["correlationId"]=>
        string(36) "1f262283-c607-4ca2-b721-2be0f74154c8"
        ["messageId"]=>
        string(36) "7DC32375-F6A0-EE69-AB07-00002B3FABC5"
        ["clientId"]=>
        string(36) "58CA5503-8BC1-9FC9-4B96-00007C8BBB7A"
        ["timestamp"]=>
        float(1506053950277)
        ["timeToLive"]=>
        NULL
        ["destination"]=>
        NULL
        ["headers"]=>
        object(stdClass)#6 (1) {
          ["DSId"]=>
          string(36) "58CA5503-8BC1-9FC9-4B96-00007C8BBB7A"
        }
        ["body"]=>
        array(1) {
          [0]=>
          object(stdClass)#2 (4) {
            ["_explicitType"]=>
            string(17) "VoIntegerProperty"
            ["id"]=>
            int(1)
            ["name"]=>
            string(1) "a"
            ["11"]=>
            string(2) "aa"
          }
        }
      }
      ["_explicitType"]=>
      string(33) "flex.messaging.io.amf.MessageBody"
    }
  }
}

