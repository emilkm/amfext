--TEST--
Read AMF3 XML and XML reference
--DESCRIPTION--
Reads XML string AMF3 format to an internal XML class.
Then reads a XML string AMF3 format to SimpleXMLElement.
The latter requires PHP build configuration options --enable-libxml --enable-simplexml

--SKIPIF--
<?php if (!extension_loaded('amf')) print 'skip'; ?>
--FILE--
<?php
include 'amf_decoder.inc';
$decoder = new AmfDecoder();
$data = unserialize(file_get_contents(__DIR__ . '/asset/value/xml-and-reference.amf3'));
$decoder->setData($data);
$decoder->setUseInternalXmlType(true);
$obj = $decoder->run();
var_dump($obj->value1);
var_dump($obj->value2);
$data = unserialize(file_get_contents(__DIR__ . '/asset/value/xmlelement-and-reference.amf3'));
$decoder->setData($data);
$decoder->setUseInternalXmlType(false);
$obj = $decoder->run();
var_dump($obj->value1);
$xmlstring = preg_replace('/\>(\n|\r|\r\n| |\t)*\</', '><', trim($obj->value1->asXML()));
print_r($xmlstring);
echo PHP_EOL;
var_dump($obj->value2);
$xmlstring = preg_replace('/\>(\n|\r|\r\n| |\t)*\</', '><', trim($obj->value2->asXML()));
print_r($xmlstring);
?>
--EXPECTF--
object(Xml)#%d (1) {
  ["data"]=>
  string(47) "<x><string>abc</string><number>123</number></x>"
}
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
object(SimpleXMLElement)#%d (2) {
  ["string"]=>
  string(3) "abc"
  ["number"]=>
  string(3) "123"
}
<?xml version="1.0"?><x><string>abc</string><number>123</number></x>
