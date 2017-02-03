--TEST--
Write AMF3 Date 
--DESCRIPTION--
Writes a date in AMF3 format.



--SKIPIF--
<?php if (!extension_loaded('amf')) print 'skip'; ?>
--FILE--
<?php
include 'amf_encoder.inc';
$encoder = new AmfEncoder();
$encoder->setAvmPlus(true);
$data = unserialize(file_get_contents(__DIR__ . '/asset/value/date.amf3'));
$obj = new stdClass();
$obj->value = new Date(1422995025123); //2015-02-04 09:23:45.123
$res = $encoder->run($obj);
echo ($res === $data) ? 'same' : 'diff';
$obj = new stdClass();
$datestr = date('Y-m-d H:i:s.', 1422995025) . 123; //2015-02-04 09:23:45.123
$obj->value = new DateTime($datestr);
$res = $encoder->run($obj);
echo ($res === $data) ? 'same' : 'diff';
?>
--EXPECT--
samesame
