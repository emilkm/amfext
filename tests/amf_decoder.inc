<?php
class AmfDecoder
{
    const REMOTE_CLASS_FIELD = '_explicitType';

    const AMF_AMF3 = 1;
    const AMF_BIGENDIAN = 2;
    const AMF_OBJECT_AS_ASSOC = 4;
    const AMF_POST_DECODE = 8;
    const AMF_AS_STRING_BUILDER = 16;
    const AMF_TRANSLATE_CHARSET = 32;
    const AMF_TRANSLATE_CHARSET_FAST = 96; //32|64
    const AMF3_NSD_ARRAY_AS_OBJECT = 128;

    const AMFE_MAP = 1;
    const AMFE_POST_OBJECT = 2;
    const AMFE_POST_XML = 3;
    const AMFE_MAP_EXTERNALIZABLE = 4;
    const AMFE_POST_BYTEARRAY = 5;
    const AMFE_TRANSLATE_CHARSET = 6;
    const AMFE_POST_DATE = 7;
    const AMFE_POST_XMLDOCUMENT = 8;
    const AMFE_VECTOR_INT = 9;
    const AMFE_VECTOR_UINT = 10;
    const AMFE_VECTOR_DOUBLE = 11;
    const AMFE_VECTOR_OBJECT = 12;

    public $decodeFlags;

    /**
     * @var bool
     */
    protected $bigEndianMachine = false;

    /**
     * Switching to AMF0 or AMF3 is done automatically during deserialization
     * upon encountering the AMF0_AMF3 marker.
     *
     * @var bool
     */
    protected $avmPlus = false;

    /**
     * @var bool
     */
    protected $useInternalDateType = true;

    /**
     * @var bool
     */
    protected $useInternalXmlType = true;

    /**
     * @var bool
     */
    protected $useInternalXmlDocumentType = true;

    /**
     * @var string The encoded data
     */
    protected $data;

    /**
     * @var int
     */
    protected $length;

    /**
     * @var int
     */
    protected $pos;

    /**
     *
     */
    public function __construct()
    {
        $this->bigEndianMachine = (pack('l', 1) === "\x00\x00\x00\x01");
        $this->decodeFlags = (!$this->bigEndianMachine ? self::AMF_BIGENDIAN : 0);
    }

    /**
     * @param string $data
     */
    public function setData(&$data)
    {
        $this->data = $data;
        $this->length = strlen($data);
        $this->pos = 0;
    }

    /**
     * Date decoded as efxphp Date if true, PHP DateTime otherwise
     *
     * @param bool $value
     */
    public function setUseInternalDateType($value)
    {
        $this->useInternalDateType = $value;
    }

    /**
     * XML decoded as efxphp Xml if true, PHP SimpleXMLElement otherwise
     *
     * @param bool $value
     */
    public function setUseInternalXmlType($value)
    {
        $this->useInternalXmlType = $value;
    }

    /**
     * XMLDocument decoded as efxphp XmlDocument if true, PHP DOMElement otherwise
     *
     * @param bool $value
     */
    public function setUseInternalXmlDocumentType($value)
    {
        $this->useInternalXmlDocumentType = $value;
    }

    /**
     * Create an instance of a generic anonymous or specific typed object.
     *
     * @param string $className
     * @return stdClass, typed object, or stdClass with remoteClassField set
     */
    protected function resolveType($className)
    {
        $clazz = 'stdClass';
        if ($className == '' || $className == 'Object') {
            $obj = new $clazz();
        } else {
            if ($pos = strpos($className, 'flex.messaging.messages.') === 0) {
                $class = substr($className, 24);
                $clasx = 'emilkm\\efxphp\\Amf\\Messages\\' . $class;
            } else {
                $clasx = str_replace('.', '\\', $className);
            }
            if (class_exists($clasx)) {
                $clazz = $clasx;
            }
            $obj = new $clazz();
            if ($clazz == 'stdClass') {
                $remoteClassField = self::REMOTE_CLASS_FIELD;
                $obj->$remoteClassField = $className;
            }
        }
        return $obj;
    }

    /**
     * Public entry point to read a top level AMF Object, such as
     * a header value or a message body.
     *
     * @return mixed
     *
     * @throws Exception
     */
    public function run()
    {
        return amf_decode($this->data, $this->decodeFlags, $this->pos, array(&$this, 'decodeCallback'));
    }

    /**
     * @param mixed $event The AMFEvent
     * @param mixed $arg
     *
     * @return {\DateTime|Types\ByteArray|\SimpleXMLElement|\stdClass|mixed}
     */
    private function decodeCallback($event, $arg)
    {
        switch ($event) {
            case self::AMFE_MAP:
                return $this->resolveType($arg);
            case self::AMFE_POST_OBJECT:
                return $arg;
            case self::AMFE_POST_DATE:
                if ($this->useInternalDateType == true) {
                    $value = new Date($arg);
                } else {
                    $timestamp = $arg / 1000;
                    $milli = round($timestamp - ($timestamp >> 0), 3) * 1000;
                    $timestamp = floor($timestamp);
                    $datestr = date('Y-m-d H:i:s.', $timestamp) . $milli;
                    $value = new DateTime($datestr, new DateTimeZone(date_default_timezone_get()));
                }

                return $value;
            case self::AMFE_POST_XML:
                if ($this->useInternalXmlType == true) {
                    $value = new Xml($arg);
                } else {
                    $value = simplexml_load_string($arg);
                }

                return $value;
            case self::AMFE_POST_XMLDOCUMENT:
                if ($this->useInternalXmlDocumentType == true) {
                    $value = new XmlDocument($arg);
                } else {
                    $value = dom_import_simplexml(simplexml_load_string($arg));
                }

                return $value;
            case self::AMFE_MAP_EXTERNALIZABLE:
                if ($arg == 'flex.messaging.io.ArrayCollection' || $arg == 'flex.messaging.io.ObjectProxy') {
                    //returning NULL means that the externalized data is used directly. For example an array collection will not be deserialized
                    //as an array collection with an _externalizedData field containing the source array. Rather it will be deserialized directly as the source array
                    return;
                } else {
                    //externalized data we don't know what to do with. log an error, return an empty object typed with the class name.
                    //note: this is due to a limitation in the C code.
                    trigger_error('Unable to read externalizable data type ' . $arg, E_USER_ERROR);

                    return 'error';
                }
                break;
            case self::AMFE_POST_BYTEARRAY:
                return new ByteArray($arg);
            case self::AMFE_VECTOR_INT:
                return new Vector(Vector::AMF3_VECTOR_INT, $arg);
            case self::AMFE_VECTOR_UINT:
                return new Vector(Vector::AMF3_VECTOR_UINT, $arg);
            case self::AMFE_VECTOR_DOUBLE:
                return new Vector(Vector::AMF3_VECTOR_DOUBLE, $arg);
            case self::AMFE_VECTOR_OBJECT:
                return new Vector(Vector::AMF3_VECTOR_OBJECT, $arg);
            default:
                throw new Exception('invalid event in decode callback : ' . $event);
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
