--TEST--
Read AMF0 XMLDocument
--DESCRIPTION--
Reads XMLDocument string AMF0 format to an internal XML class.
Then reads a XMLDocument string AMF0 format to SimpleXMLElement.
The latter requires PHP build configuration options --enable-libxml --enable-simplexml

--SKIPIF--
<?php if (!extension_loaded('amf')) print 'skip'; ?>
--FILE--
<?php
include 'amf_decoder.inc';
$decoder = new AmfDecoder();
$data = unserialize(file_get_contents(__DIR__ . '/asset/value/xmldocument.amf0'));
$decoder->setData($data);
$decoder->setUseInternalXmlType(true);
$obj = $decoder->run();
var_dump($obj->value);
$data = unserialize(file_get_contents(__DIR__ . '/asset/value/domelement.amf0'));
$decoder->setData($data);
$decoder->setUseInternalXmlType(false);
$obj = $decoder->run();
var_dump($obj->value);
$xmlstring = preg_replace('/\>(\n|\r|\r\n| |\t)*\</', '><', trim($obj->value->asXML()));
print_r($xmlstring);
?>
--EXPECTF--
object(Xml)#%d (1) {
  ["data"]=>
  string(47) "<x><string>abc</string><number>123</number></x>"
}
object(SimpleXMLElement)#%d (2) {
  ["string"]=>
  string(3) "abc"
  ["number"]=>
  string(3) "123"
}
<?xml version="1.0"?><x><string>abc</string><number>123</number></x>
