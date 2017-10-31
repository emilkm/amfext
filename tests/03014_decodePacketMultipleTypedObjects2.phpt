--TEST--
Read AMF0 Number
--DESCRIPTION--
Reads a number in AMF0 format.



--SKIPIF--
<?php if (!extension_loaded('amf')) print 'skip'; ?>
--FILE--
<?php
include __DIR__ . '/asset/value/RD_LOOKUPRECORD.php';
include 'amf_decoder.inc';
$decoder = new AmfDecoder();
$data = unserialize(file_get_contents(__DIR__ . '/asset/request/packet-typed-object-multiple2.amf3'));
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
      string(2) "/3"
      ["data"]=>
      array(1) {
        [0]=>
        object(flex\messaging\messages\RemotingMessage)#%d (9) {
          ["source"]=>
          string(45) "Artena.Modules.PublicTrust.DB.SRV_PUBLICTRUST"
          ["operation"]=>
          string(18) "findStudyContracts"
          ["messageId"]=>
          string(36) "AA5780D8-B1AA-42B3-EDDE-54FEA2175DB2"
          ["clientId"]=>
          string(36) "214D2E34-964C-1449-EBD8-0000246145E4"
          ["timestamp"]=>
          int(0)
          ["timeToLive"]=>
          int(0)
          ["destination"]=>
          string(8) "amfpdest"
          ["headers"]=>
          object(stdClass)#5 (3) {
            ["DSId"]=>
            string(36) "214D2E34-964C-1449-EBD8-0000246145E4"
            ["DSEndpoint"]=>
            string(4) "amfp"
            ["DSRequestTimeout"]=>
            int(300)
          }
          ["body"]=>
          array(1) {
            [0]=>
            array(2) {
              [0]=>
              array(2) {
                [0]=>
                object(Artena\Modules\Application\DB\DataSet\VO\RD_LOOKUPRECORD)#%d (6) {
                  ["Key"]=>
                  string(2) "01"
                  ["Description"]=>
                  string(14) "01 Main Campus"
                  ["Visible"]=>
                  int(1)
                  ["Children"]=>
                  NULL
                  ["_selectable"]=>
                  int(0)
                  ["checked"]=>
                  int(0)
                }
                [1]=>
                object(Artena\Modules\Application\DB\DataSet\VO\RD_LOOKUPRECORD)#%d (6) {
                  ["Key"]=>
                  string(2) "  "
                  ["Description"]=>
                  string(5) " None"
                  ["Visible"]=>
                  int(1)
                  ["Children"]=>
                  NULL
                  ["_selectable"]=>
                  int(0)
                  ["checked"]=>
                  int(0)
                }
              }
              [1]=>
              string(2) "AE"
            }
          }
        }
      }
    }
  }
}