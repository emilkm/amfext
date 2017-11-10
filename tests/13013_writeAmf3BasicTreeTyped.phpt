--TEST--
Write AMF3 basic tree typed
--DESCRIPTION--
Writes a basic tree typed in AMF3 format.



--SKIPIF--
<?php if (!extension_loaded('amf')) print 'skip'; ?>
--FILE--
<?php
include 'amf_encoder.inc';
$encoder = new AmfEncoder();
$encoder->setAvmPlus(true);
$data = unserialize(file_get_contents(__DIR__ . '/asset/value/basic-tree-typed.amf3'));
$parent = new stdClass();
$parent->_explicitType = 'SomeClass';
$parent->parent = null;
$child = new stdClass();
$child->_explicitType = 'SomeClass';
$child->parent = $parent;
$child->children = null;
$parent->children = [$child];
$res = $encoder->run($parent);
echo ($res === $data) ? 'same' : 'diff';
?>
--EXPECT--
same
