--TEST--
Write AMF3 XML Document
--DESCRIPTION--
Writes a XML Document string in AMF3 format from a userland XMLDocument class.
Then writes a XML Document string AMF3 format from DOMElement.
The latter requires PHP build configuration options --enable-libxml --enable-simplexml --enable-dom

--SKIPIF--
<?php if (!extension_loaded('amf')) print 'skip'; ?>
--FILE--
<?php
include 'amf_encoder.inc';
$encoder = new AmfEncoder();
$encoder->setAvmPlus(true);
$data = unserialize(file_get_contents(__DIR__ . '/asset/value/xmldocument.amf3'));
$obj = new stdClass();
$obj->value = new XmlDocument('<x><string>abc</string><number>123</number></x>');
$res = $encoder->run($obj);
echo ($res === $data) ? 'same' : 'diff';
$data = unserialize(file_get_contents(__DIR__ . '/asset/value/domelement.amf3'));
$obj = new stdClass();
$obj->value = dom_import_simplexml(simplexml_load_string('<x><string>abc</string><number>123</number></x>'));
$res = $encoder->run($obj);
echo ($res === $data) ? 'same' : 'diff';
?>
--EXPECT--
samesame
