<?php
require_once __DIR__ . '/amf_classes.inc';
require_once __DIR__ . '/amf_messages.inc';

class AmfDecoder
{
    const REMOTE_CLASS_FIELD = '_explicitType';

    const AMF_AMF3 = 1;
    const AMF_BIGENDIAN = 2;
    const AMF_OBJECT_AS_ASSOC = 4;
    const AMF3_NSND_ARRAY_AS_OBJECT = 8;
    const AMF_USE_RLAND_DATE = 16;
    const AMF_USE_RLAND_XML = 32;
    const AMF_USE_RLAND_XMLDOCUMENT = 64;

    const AMFC_DATE = 0;
    const AMFC_BYTEARRAY = 1;
    const AMFC_XML = 2;
    const AMFC_XMLDOCUMENT = 3;
    const AMFC_VECTOR_INT = 4;
    const AMFC_VECTOR_UINT = 5;
    const AMFC_VECTOR_DOUBLE = 6;
    const AMFC_VECTOR_OBJECT = 7;
    const AMFC_EXTERNALIZABLE = 8;

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
     * Decode typed object or associative array.
     *
     * @var bool
     */
    protected $decodeAmfObjectAsArray = false;

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
    public $pos;

    /**
     *
     */
    public function __construct()
    {
        //spl_autoload_register(array($this, 'loadClass'), true, false);
        $this->bigEndianMachine = (pack('l', 1) === "\x00\x00\x00\x01");
    }

    /*protected function __destruct()
    {
        spl_autoload_unregister(array($this, 'loadClass'));
    }*/

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
     * Decode typed object or associative array.
     *
     * @param bool $value
     */
    public function setDecodeAmfObjectAsArray($value)
    {
        $this->decodeAmfObjectAsArray = $value;
    }

    protected function loadClass($className)
    {
        if (strpos($className, 'flex\\messaging\\messages\\') === 0) {
            if (file_exists($file = __DIR__ . '\\asset\\' . $className . '.php')) {
                require_once $file;
            }
        }
    }

    /**
     * Create an instance of a generic anonymous or specific typed object.
     *
     * @param string $className
     * @return stdClass, typed object, or stdClass with remoteClassField set
     */
    protected function resolveType($className)
    {
        if ($this->decodeAmfObjectAsArray && strpos($className, 'flex.messaging.messages.') === false) {
            $arr = [];
            if ($className != '' && $className != 'Object') {
                $arr[self::REMOTE_CLASS_FIELD] = $className;
            }
            return $arr;
        }

        try {
            $clazz = 'stdClass';
            if ($className == '' || $className == 'Object') {
                $obj = new $clazz();
            } else {
                $clasx = str_replace('.', '\\', $className);
                if (class_exists($clasx)) {
                    $clazz = $clasx;
                }
                $obj = new $clazz();
                if ($clazz == 'stdClass') {
                    $remoteClassField = self::REMOTE_CLASS_FIELD;
                    $obj->$remoteClassField = $className;
                }
            }
        } catch (Exception | Error $e) {
            $obj = new stdClass();
            $remoteClassField = self::REMOTE_CLASS_FIELD;
            $obj->$remoteClassField = $className;
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
        $userlandTypes = [
            'Date' => self::AMFC_DATE,
            'ByteArray' => self::AMFC_BYTEARRAY,
            'Xml' => self::AMFC_XML,
            'XmlDocument' => self::AMFC_XMLDOCUMENT,
            'Vector' => self::AMFC_VECTOR_OBJECT
        ];
        $this->decodeFlags = ((!$this->bigEndianMachine ? self::AMF_BIGENDIAN : 0)
            | ($this->decodeAmfObjectAsArray ? self::AMF_OBJECT_AS_ASSOC : 0)
            | ($this->useInternalDateType ? self::AMF_USE_RLAND_DATE : 0)
        );
        return amf_decode($this->data, $this->pos, $this->decodeFlags, $userlandTypes, array(&$this, 'decodeCallback'));
    }

    /**
     * @param mixed $event The AMFEvent
     * @param mixed $arg
     *
     * @return {\DateTime|Types\ByteArray|\SimpleXMLElement|\stdClass|mixed}
     */
    private function decodeCallback($type, $arg)
    {
        switch ($type) {
            case self::AMFC_DATE:
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
            case self::AMFC_BYTEARRAY:
                return new ByteArray($arg);
            case self::AMFC_XML:
                if ($this->useInternalXmlType == true) {
                    $value = new Xml($arg);
                } else {
                    $value = simplexml_load_string($arg);
                }

                return $value;
            case self::AMFC_XMLDOCUMENT:
                if ($this->useInternalXmlDocumentType == true) {
                    $value = new XmlDocument($arg);
                } else {
                    $value = dom_import_simplexml(simplexml_load_string($arg));
                }

                return $value;
            case self::AMFC_VECTOR_INT:
                return new Vector(Vector::AMF3_VECTOR_INT, $arg);
            case self::AMFC_VECTOR_UINT:
                return new Vector(Vector::AMF3_VECTOR_UINT, $arg);
            case self::AMFC_VECTOR_DOUBLE:
                return new Vector(Vector::AMF3_VECTOR_DOUBLE, $arg);
            case self::AMFC_VECTOR_OBJECT:
                return new Vector(Vector::AMF3_VECTOR_OBJECT, $arg);
            case self::AMFC_EXTERNALIZABLE:
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
            default:
                throw new Exception('invalid type in decode callback : ' . $type);
        }
    }

    /**
     * @param mixed $data
     *
     * @return ActionMessage
     */
    public function readMessage(&$data)
    {
        $this->setData($data);
        $version = $this->readShort();
        $message = new ActionMessage($version);
        // Read headers
        $headerCount = $this->readShort();
        for ($i = 0; $i < $headerCount; ++$i) {
            $message->headers[] = $this->readHeader();
        }
        // Read bodies
        $bodyCount = $this->readShort();
        for ($i = 0; $i < $bodyCount; ++$i) {
            $message->bodies[] = $this->readBody();
        }
        return $message;
    }

    /**
     * Deserialize a message header
     *
     * @return MessageHeader
     */
    protected function readHeader()
    {
        $header = new MessageHeader();
        $header->name = $this->readUtf();
        $header->mustUnderstand = $this->readBoolean();
        $this->skipBytes(4); // skip header length
        $header->data = $this->run();
        return $header;
    }

    /**
     * Deserialize a message body
     *
     * @return MessageBody
     */
    protected function readBody()
    {
        $message = new MessageBody();
        $message->targetURI = $this->readUtf();
        $message->responseURI = $this->readUtf();
        $this->skipBytes(4); // skip message length
        $message->data = $this->run();
        return $message;
    }

    /**
     * @param int $n The number of bytes to skip
     */
    public function skipBytes($n)
    {
        if ($this->pos + $n > $this->length) {
            throw new Exception('Cannot skip past the end of the data.');
        }
        $this->pos += $n;
    }

    /**
     * @param int $n The number of bytes to read
     * @return string The next $n bytes as a string
     */
    public function readBytes($n)
    {
        if ($this->pos + $n > $this->length) {
            throw new Exception('Cannot read past the end of the data.');
        }
        $value = '';
        for ($i = 0; $i < $n; $i++) {
            $value .= $this->data[$this->pos + $i];
        }
        $this->pos += $n;
        return $value;
    }

    /**
     * Does not advance the current position
     *
     * @param int $n The number of bytes to add to current position (default 0)
     * @return int The byte at the current position as an integer
     */
    public function peekByte($n = 0)
    {
        if ($this->pos + $n > $this->length) {
            throw new Exception('Cannot read past the end of the data.');
        }
        return ord($this->data[$this->pos + $n]);
    }

    /**
     * @return int The next byte as an integer
     */
    public function readByte()
    {
        if ($this->pos + 1 > $this->length) {
            throw new Exception('Cannot read past the end of the data.');
        }
        return ord($this->data[$this->pos++]);
    }

    /**
     * @return bool The next byte as a boolean
     */
    public function readBoolean()
    {
        return $this->readByte() == 1;
    }

    /**
     * @return int The next 2 bytes as an integer
     */
    public function readShort()
    {
        if ($this->pos + 2 > $this->length) {
            throw new Exception('Cannot read past the end of the data.');
        }
        return ((ord($this->data[$this->pos++]) << 8) |
                ord($this->data[$this->pos++]));
    }

    /**
     * @return int The next 4 bytes as an integer
     */
    public function readInt()
    {
        if ($this->pos + 4 > $this->length) {
            throw new Exception('Cannot read past the end of the data.');
        }
        return ((ord($this->data[$this->pos++]) << 24) |
                (ord($this->data[$this->pos++]) << 16) |
                (ord($this->data[$this->pos++]) << 8) |
                ord($this->data[$this->pos++]));
    }

    /**
     * @return float The next 8 bytes as a float
     */
    public function readDouble()
    {
        $value = $this->readBytes(8);
        if (!$this->bigEndianMachine) {
            $value = strrev($value);
        }
        $zz = unpack('dflt', $value);
        return $zz['flt'];
    }

    /**
     * @return string The UTF8 Unicode string
     */
    public function readUtf()
    {
        $len = $this->readShort();
        return $this->readBytes($len);
    }
}


