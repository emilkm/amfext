<?php
class AmfEncoder
{
    const REMOTE_CLASS_FIELD = '_explicitType';

    const AMF_AMF3 = 1;
    const AMF_BIGENDIAN = 2;
    const AMF0_ASSOC = 4;
    const AMF_POST_DECODE = 8;
    const AMF_AS_STRING_BUILDER = 16;
    const AMF_TRANSLATE_CHARSET = 32;
    const AMF_TRANSLATE_CHARSET_FAST = 96; //32|64
    const AMF3_NSND_ARRAY_AS_OBJECT = 128;

    const AMFC_RAW = 0;
    const AMFC_XML = 1;
    const AMFC_OBJECT = 2;
    const AMFC_TYPEDOBJECT = 3;
    const AMFC_ANY = 4;
    const AMFC_ARRAY = 5;
    const AMFC_NONE = 6;
    const AMFC_BYTEARRAY = 7;
    const AMFC_EXTERNAL = 8;
    const AMFC_DATE = 9;
    const AMFC_XMLDOCUMENT = 10;
    const AMFC_VECTOR_OBJECT = 11;

    public $encodeFlags;

    /**
     * @var bool
     */
    protected $bigEndianMachine = false;

    /**
     * AVM+ Encoding.
     *
     * @var bool
     */
    protected $avmPlus = false;

    /**
     * Encode non-strict non-dense array as anonymous object or associative array.
     *
     * @var bool
     */
    protected $amf3nsndArrayAsObject = false;

    /**
     * @var string
     */
    public $data;

    /**
     * Sets the bigEndianMachine property of the Output[Ext] instance
     */
    public function __construct()
    {
        $this->bigEndianMachine = (pack('l', 1) === "\x00\x00\x00\x01");
    }

    /**
     * @return string The encoded data
     */
    public function getData()
    {
        return $this->data;
    }

    /**
     * Set to true if the AMF0 stream should switch to use AMF3 on encountering
     * the first complex Object during serialization.
     *
     * @param bool $value
     */
    public function setAvmPlus($value)
    {
        $this->avmPlus = $value;
    }

    /**
     * Encode string, mixed, sparse, or negative index arrays as anonymous object,
     * associative array otherwise.
     *
     * @param bool $value
     */
    public function encodeAmf3nsndArrayAsObject($value)
    {
        $this->amf3nsndArrayAsObject = $value;
    }

    /**
     * Public entry point to write a top level AMF Object, such as
     * a header value or a message body.
     *
     * If we're using AMF 3, and a complex object is encountered,
     * encoding is switched to AVM+ format.
     *
     * @param mixed $value The object to write
     */
    public function run($value)
    {
        $this->encodeFlags = ($this->avmPlus ? self::AMF_AMF3 : 0)
            | (!$this->bigEndianMachine ? self::AMF_BIGENDIAN : 0)
            | ($this->amf3nsndArrayAsObject ? self::AMF3_NSND_ARRAY_AS_OBJECT : 0);
        return amf_encode($value, $this->encodeFlags, array(&$this, 'encodeCallback'));
    }

    private function encodeCallback($value)
    {
        if (is_object($value)) {
            if ($value instanceof Date) {
                $amfdate = (float) $value->timestamp . $value->milli + 0.0;
                return array($amfdate, self::AMFC_DATE);
            } elseif ($value instanceof DateTime) {
                $amfdate = (float) $value->getTimeStamp() . floor($value->format('u') / 1000) + 0.0;
                return array($amfdate, self::AMFC_DATE);
            } elseif ($value instanceof ByteArray) {
                return array($value->data, self::AMFC_BYTEARRAY);
            } elseif ($value instanceof Xml) {
                return array($value->data, self::AMFC_XML);
            } elseif ($value instanceof SimpleXMLElement) {
                $xmlstring = preg_replace('/\>(\n|\r|\r\n| |\t)*\</', '><', trim($value->asXML()));
                return array($xmlstring, self::AMFC_XML);
            } elseif ($value instanceof XmlDocument) {
                return array($value->data, self::AMFC_XMLDOCUMENT);
            } elseif ($value instanceof DOMElement) {
                $xmlstring = preg_replace('/\>(\n|\r|\r\n| |\t)*\</', '><', trim($value->ownerDocument->saveXML($value)));
                return array($xmlstring, self::AMFC_XMLDOCUMENT);
            } elseif ($value instanceof Vector) {
                return array($value, self::AMFC_VECTOR_OBJECT);
            } else {
                $className = get_class($value);
                $remoteClassField = self::REMOTE_CLASS_FIELD;
                if (isset($value->$remoteClassField)) {
                    $className = $value->$remoteClassField;
                    unset($value->$remoteClassField);
                } else {
                    $className = str_replace('\\', '.', $className);
                }
                if ($className == '') {
                    return array($value, self::AMFC_OBJECT, $className);
                } else {
                    return array($value, self::AMFC_TYPEDOBJECT, $className);
                }
            }
        }
    }
}

class Date
{
    /**
     * @var int
     */
    public $timestamp;

    /**
     * @var string
     */
    public $milli;

    /**
     * AMF serialized date as the number of milliseconds elapsed
     * since the epoch of midnight on 1st Jan 1970
     *
     * @param int $amfdate
     */
    public function __construct($amfdate)
    {
        $timestamp = $amfdate / 1000;
        $this->milli = round($timestamp - ($timestamp >> 0), 3) * 1000;
        $this->timestamp = floor($timestamp);
    }

    /**
     * @return string
     */
    public function __toString()
    {
        return date('Y-m-d H:i:s.', $this->timestamp) . $this->milli;
    }
}

class Xml
{
    /**
     * @var string XML data
     */
    public $data;

    /**
     * @param string $xmlstring
     */
    public function __construct($xmlstring)
    {
        $this->data = preg_replace('/\>(\n|\r|\r\n| |\t)*\</', '><', trim($xmlstring));
    }

    /**
     * @return string
     */
    public function __toSring()
    {
        return $this->data;
    }
}

class XmlDocument
{
    /**
     * @var string XML data
     */
    public $data;

    /**
     * @param string $xmlstring
     */
    public function __construct($xmlstring)
    {
        $this->data = preg_replace('/\>(\n|\r|\r\n| |\t)*\</', '><', trim($xmlstring));
    }

    /**
     * @return string
     */
    public function __toSring()
    {
        return $this->data;
    }
}

class ByteArray
{
    /**
     * @var string ByteString data
     */
    public $data;

    /**
     * @param string $data
     */
    public function __construct($data)
    {
        $this->data = $data;
    }

    /**
     * @return string
     */
    public function __toSring()
    {
        return $this->data;
    }
}

class Vector
{
    const AMF3_VECTOR_INT           = 13;
    const AMF3_VECTOR_UINT          = 14;
    const AMF3_VECTOR_DOUBLE        = 15;
    const AMF3_VECTOR_OBJECT        = 16;

    /**
     * @var int The type of vector.
     */
    public $type = self::AMF3_VECTOR_OBJECT;

    /**
     * @var bool fixed or variable length
     */
    public $fixed = false;

    /**
     * @var array of primitives or objects
     */
    public $data;

    /**
     * @param int   $type
     * @param array $data
     */
    public function __construct($type, $data)
    {
        $this->type = $type;
        $this->data = $data;
    }

    /**
     * @return string
     */
    public function __toSring()
    {
        $typestr = 'object';
        switch ($this->type) {
            case self::AMF3_VECTOR_INT:
                $typestr = 'int';
                break;
            case self::AMF3_VECTOR_UINT:
                $typestr = 'uint';
                break;
            case self::AMF3_VECTOR_DOUBLE:
                $typestr = 'double';
                break;
            case self::AMF3_VECTOR_OBJECT:
                $typestr = 'object';
                break;
        }
        return 'A ' . ($this->fixed ? 'fixed' : 'variable') . ' Vector ' . $typestr;
    }
}
