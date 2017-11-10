--TEST--
Write AMF3 XML
--DESCRIPTION--
Writes a XML string in AMF3 format from a userland XML class.
Then writes a XML string AMF3 format from SimpleXMLElement.
The latter requires PHP build configuration options --enable-libxml --enable-simplexml --enable-dom

--SKIPIF--
<?php if (!extension_loaded('amf')) print 'skip'; ?>
--FILE--
<?php
include 'amf_encoder.inc';
$encoder = new AmfEncoder();
$encoder->setAvmPlus(true);
$data = unserialize(file_get_contents(__DIR__ . '/asset/value/xml-and-reference.amf3'));
$obj = new stdClass();
$xmlobj = new Xml('<x><string>abc</string><number>123</number></x>');
$obj->value1 = $xmlobj;
$obj->value2 = $xmlobj;
$res = $encoder->run($obj);
echo ($res === $data) ? 'same' : 'diff';
$data = unserialize(file_get_contents(__DIR__ . '/asset/value/xmlelement-and-reference.amf3'));
$obj = new stdClass();
$xmlobj = simplexml_load_string('<x><string>abc</string><number>123</number></x>');
$obj->value1 = $xmlobj;
$obj->value2 = $xmlobj;
$res = $encoder->run($obj);
echo ($res === $data) ? 'same' : 'diff';
?>
--EXPECT--
samesame
