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
    const AMFC_VECTOR = 4;
    const AMFC_EXTERNALIZABLE = 5;

	public $userlandTypes = [
        'Date' => self::AMFC_DATE,
        'ByteArray' => self::AMFC_BYTEARRAY,
        'Xml' => self::AMFC_XML,
        'XmlDocument' => self::AMFC_XMLDOCUMENT,
        'Vector' => self::AMFC_VECTOR
    ];

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
    protected $useRlandDateType = true;

    /**
     * @var bool
     */
    protected $useRlandXmlType = true;

    /**
     * @var bool
     */
    protected $useRlandXmlDocumentType = true;

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
    public function setUseRlandDateType($value)
    {
        $this->useRlandDateType = $value;
    }

    /**
     * XML decoded as efxphp Xml if true, PHP SimpleXMLElement otherwise
     *
     * @param bool $value
     */
    public function setUseRlandXmlType($value)
    {
        $this->useRlandXmlType = $value;
    }

    /**
     * XMLDocument decoded as efxphp XmlDocument if true, PHP DOMElement otherwise
     *
     * @param bool $value
     */
    public function setUseRlandXmlDocumentType($value)
    {
        $this->useRlandXmlDocumentType = $value;
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
        $this->decodeFlags = ((!$this->bigEndianMachine ? self::AMF_BIGENDIAN : 0)
            | ($this->decodeAmfObjectAsArray ? self::AMF_OBJECT_AS_ASSOC : 0)
            | ($this->useRlandDateType ? self::AMF_USE_RLAND_DATE : 0)
        );
        return amf_decode($this->data, $this->pos, $this->decodeFlags, $this->userlandTypes);
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


