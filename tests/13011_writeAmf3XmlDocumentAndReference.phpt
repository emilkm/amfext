--TEST--
Write AMF3 XML Document and reference
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
$data = unserialize(file_get_contents(__DIR__ . '/asset/value/xmldocument-and-reference.amf3'));
$obj = new stdClass();
$xmlobj = new XmlDocument('<x><string>abc</string><number>123</number></x>');
$obj->value1 = $xmlobj;
$obj->value2 = $xmlobj;
$res = $encoder->run($obj);
echo ($res === $data) ? 'same' : 'diff';
$data = unserialize(file_get_contents(__DIR__ . '/asset/value/domelement-and-reference.amf3'));
$obj = new stdClass();
$xmlobj = dom_import_simplexml(simplexml_load_string('<x><string>abc</string><number>123</number></x>'));
$obj->value1 = $xmlobj;
$obj->value2 = $xmlobj;
$res = $encoder->run($obj);
echo ($res === $data) ? 'same' : 'diff';
?>
--EXPECT--
samesame
