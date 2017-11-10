--TEST--
Write AMF0 XML
--DESCRIPTION--
Writes a XML string in AMF0 format from a userland XML class.
Then writes a XML string AMF0 format from SimpleXMLElement.
The latter requires PHP build configuration options --enable-libxml --enable-simplexml --enable-dom

--SKIPIF--
<?php if (!extension_loaded('amf')) print 'skip'; ?>
--FILE--
<?php
include 'amf_encoder.inc';
$encoder = new AmfEncoder();
$encoder->setAvmPlus(false);
$data = unserialize(file_get_contents(__DIR__ . '/asset/value/xml.amf0'));
$obj = new stdClass();
$obj->value = new Xml('<x><string>abc</string><number>123</number></x>');
$res = $encoder->run($obj);
echo ($res === $data) ? 'same' : 'diff';
$data = unserialize(file_get_contents(__DIR__ . '/asset/value/xmlelement.amf0'));
$obj = new stdClass();
$obj->value = simplexml_load_string('<x><string>abc</string><number>123</number></x>');
$res = $encoder->run($obj);
echo ($res === $data) ? 'same' : 'diff';
?>
--EXPECT--
samesame
