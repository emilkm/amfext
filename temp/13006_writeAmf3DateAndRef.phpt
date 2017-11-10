--TEST--
Write AMF3 Date and Date reference 
--DESCRIPTION--
Reads a date, and a reference to date, in AMF3 format.



--SKIPIF--
<?php if (!extension_loaded('amf')) print 'skip'; ?>
--FILE--
<?php
include 'amf_encoder.inc';
$encoder = new AmfEncoder();
$encoder->setAvmPlus(true);
$data = unserialize(file_get_contents(__DIR__ . '/asset/value/date-and-reference.amf3'));
$obj = new stdClass();
$adate = new Date(1422995025123); //2015-02-04 09:23:45.123
$obj->value1 = $adate;
$obj->value2 = $adate;
$res = $encoder->run($obj);
echo ($res === $data) ? 'same' : 'diff';
$obj = new stdClass(); 
$datestr = date('Y-m-d H:i:s.', 1422995025) . 123; //2015-02-04 09:23:45.123
$bdate = new DateTime($datestr);
$obj->value1 = $bdate;
$obj->value2 = $bdate;
$res = $encoder->run($obj);
echo ($res === $data) ? 'same' : 'diff';
?>
--EXPECT--
samesame
