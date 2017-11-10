--TEST--
Write AMF3 simple object graph
--DESCRIPTION--
Writes a simple object graph in AMF3 format.



--SKIPIF--
<?php if (!extension_loaded('amf')) print 'skip'; ?>
--FILE--
<?php
include 'amf_encoder.inc';
$encoder = new AmfEncoder();
$encoder->setAvmPlus(true);
$data = unserialize(file_get_contents(__DIR__ . '/asset/value/simple-object-graph.amf3'));
$obj = new stdClass();
$arr = array();
$a = new stdClass();
$a->name = 'a';
$a->parent = null;
$a->children = array();
$arr[] = $a;
$a1 = new stdClass();
$a1->name = 'a1';
$a1->parent = $a;
$a1->children = null;
$a->children[] = $a1;
$obj->value = $arr;
$res = $encoder->run($obj);
echo ($res === $data) ? 'same' : 'diff';
?>
--EXPECT--
same
