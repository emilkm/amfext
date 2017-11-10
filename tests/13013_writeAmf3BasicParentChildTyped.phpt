--TEST--
Write AMF3 parent child typd
--DESCRIPTION--
Writes a basic parent child typed in AMF3 format.



--SKIPIF--
<?php if (!extension_loaded('amf')) print 'skip'; ?>
--FILE--
<?php
include 'amf_encoder.inc';
$encoder = new AmfEncoder();
$encoder->setAvmPlus(true);
$data = unserialize(file_get_contents(__DIR__ . '/asset/value/basic-parent-child-typed.amf3'));
$parent = new stdClass();
$parent->_explicitType = 'SomeClass';
$parent->parent = null;
$child = new stdClass();
$child->_explicitType = 'SomeClass';
$child->parent = $parent;
$child->child = null;
$parent->child = $child;
$res = $encoder->run($parent);
echo ($res === $data) ? 'same' : 'diff';
?>
--EXPECT--
same
