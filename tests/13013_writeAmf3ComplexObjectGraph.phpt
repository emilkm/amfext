--TEST--
Write AMF3 complex object graph
--DESCRIPTION--
Writes a complex object graph in AMF3 format.



--SKIPIF--
<?php if (!extension_loaded('amf')) print 'skip'; ?>
--FILE--
<?php
include 'amf_encoder.inc';
$encoder = new AmfEncoder();
$encoder->setAvmPlus(true);
$data = unserialize(file_get_contents(__DIR__ . '/asset/value/complex-object-graph.amf3'));
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
$b = new stdClass();
$b->name = 'b';
$b->parent = null;
$b->children = array();
$arr[] = $b;
$b1 = new stdClass();
$b1->name = 'b1';
$b1->parent = $b;
$b1->children = array();
$b->children[] = $b1;
$bb1 = new stdClass();
$bb1->name = 'bb1';
$bb1->parent = $b1;
$bb1->children = array();
$b1->children[] = $bb1;
$obj->value = $arr;
$res = $encoder->run($obj);
echo ($res === $data) ? 'same' : 'diff';
?>
--EXPECT--
same
