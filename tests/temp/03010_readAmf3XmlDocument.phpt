--TEST--
Read AMF3 XMLDocument
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
$data = unserialize(file_get_contents(__DIR__ . '/asset/value/xmldocument.amf3'));
$decoder->setData($data);
$decoder->setUseInternalXmlDocumentType(true);
$obj = $decoder->run();
var_dump($obj->value);
$data = unserialize(file_get_contents(__DIR__ . '/asset/value/domelement.amf3'));
$decoder->setData($data);
$decoder->setUseInternalXmlDocumentType(false);
$obj = $decoder->run();
var_dump(get_class($obj->value));
$xmlstring = preg_replace('/\>(\n|\r|\r\n| |\t)*\</', '><', trim($obj->value->ownerDocument->saveXML($obj->value)));
print_r($xmlstring);
?>
--EXPECTF--
object(XmlDocument)#%d (1) {
  ["data"]=>
  string(47) "<x><string>abc</string><number>123</number></x>"
}
string(10) "DOMElement"
<x><string>abc</string><number>123</number></x>
