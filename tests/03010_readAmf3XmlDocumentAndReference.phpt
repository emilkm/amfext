--TEST--
Read AMF3 XMLDocument and XMLDocument reference
--DESCRIPTION--
Reads XMLDocument string AMF3 format to an internal XML class.
Then reads a XMLDocument string AMF3 format to DOMElement.
The latter requires PHP build configuration options --enable-libxml --enable-simplexml --enable-dom

--SKIPIF--
<?php if (!extension_loaded('amf')) print 'skip'; ?>
--FILE--
<?php
include 'amf_decoder.inc';
$decoder = new AmfDecoder();
$data = unserialize(file_get_contents(__DIR__ . '/asset/value/xmldocument-and-reference.amf3'));
$decoder->setData($data);
$decoder->setUseInternalXmlDocumentType(true);
$obj = $decoder->run();
var_dump($obj->value1);
var_dump($obj->value2);
$data = unserialize(file_get_contents(__DIR__ . '/asset/value/domelement-and-reference.amf3'));
$decoder->setData($data);
$decoder->setUseInternalXmlDocumentType(false);
$obj = $decoder->run();
var_dump(get_class($obj->value1));
$xmlstring = preg_replace('/\>(\n|\r|\r\n| |\t)*\</', '><', trim($obj->value1->ownerDocument->saveXML($obj->value1)));
print_r($xmlstring);
echo PHP_EOL;
var_dump(get_class($obj->value2));
$xmlstring = preg_replace('/\>(\n|\r|\r\n| |\t)*\</', '><', trim($obj->value2->ownerDocument->saveXML($obj->value2)));
print_r($xmlstring);
?>
--EXPECTF--
object(XmlDocument)#%d (1) {
  ["data"]=>
  string(47) "<x><string>abc</string><number>123</number></x>"
}
object(XmlDocument)#%d (1) {
  ["data"]=>
  string(47) "<x><string>abc</string><number>123</number></x>"
}
string(10) "DOMElement"
<x><string>abc</string><number>123</number></x>
string(10) "DOMElement"
<x><string>abc</string><number>123</number></x>
