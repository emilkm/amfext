<?php
require_once __DIR__ . '/amf_classes.inc';
require_once __DIR__ . '/amf_messages.inc';

class AmfEncoder
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
        //spl_autoload_register(array($this, 'loadClass'), true, false);
        $this->bigEndianMachine = (pack('l', 1) === "\x00\x00\x00\x01");
    }

    /*protected function __destruct()
    {
        spl_autoload_unregister(array($this, 'loadClass'));
    }*/

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

    /*protected function loadClass($className)
    {
        if (strpos($className, 'flex\\messaging\\messages\\') === 0) {
            if (file_exists($file = __DIR__ . '\\asset\\' . $className . '.php')) {
                require_once $file;
            }
        }
    }*/

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
        return amf_encode($value, $this->encodeFlags, $this->userlandTypes);
    }

    /**
     * @param mixed $version AMF versin 0 or 3
     * @param mixed $sequence Message sequence number
     * @param mixed $operations Associative array: source, operation, body
     * @return ActionMessage
     */
    public function createRemotingActionMessage($version, $sequence, $operations)
    {
        $actionMessage = new ActionMessage($version);

        foreach ($operations as $operation) {
            $messageBody = new MessageBody(null, '/' . $sequence);

            $remotingMessage = new flex\messaging\messages\RemotingMessage(
                'F9F98C89-5099-E7BF-7997-9B41FC79A0D4',
                'efxphp',
                $operation['source'],
                $operation['operation'],
                null
            );

            //reset properties manually so we can compare output
            $remotingMessage->messageId = '63FCE70D-F447-ED49-83E6-00001695D4AF';
            $remotingMessage->timestamp = round(1437179933687);
            $remotingMessage->body = $operation['body'];

            $messageBody->data = $remotingMessage;
            $actionMessage->bodies[0] = $messageBody;

            $sequence++;
        }

        return $actionMessage;
    }

    /**
     * @param int $version AMF versin 0 or 3
     * @param int $sequence Message sequence number
     * @param array $bodies Array of bodies
     * @return ActionMessage
     */
    public function createAcknowledgeActionMessage($version, $sequence, $bodies)
    {
        $actionMessage = new ActionMessage($version);

        foreach ($bodies as $body) {
            $messageBody = new MessageBody('/' . $sequence . '/onResult');

            $acknowledgeMessage = new flex\messaging\messages\AcknowledgeMessage(null);

            //reset properties manually so we can compare output
            $acknowledgeMessage->clientId       = '58CA5503-8BC1-9FC9-4B96-00007C8BBB7A';
            $acknowledgeMessage->messageId      = '7DC32375-F6A0-EE69-AB07-00002B3FABC5';
            $acknowledgeMessage->correlationId  = '1f262283-c607-4ca2-b721-2be0f74154c8';
            $acknowledgeMessage->timestamp      = round(1506053950277);
            $acknowledgeMessage->headers        = (object) array('DSId' => $acknowledgeMessage->clientId);

            $acknowledgeMessage->body           = $body;

            $messageBody->data = $acknowledgeMessage;
            $actionMessage->bodies[0] = $messageBody;

            $sequence++;
        }

        return $actionMessage;
    }

    /**
     * @param ActionMessage $message
     *
     * @return string The serialized message
     */
    public function writeMessage($message)
    {
        $this->data = '';
        $this->setAvmPlus($message->version >= 3);
        // Write packet header
        $this->writeShort($message->version);
        // Write headers
        $headerCount = $message->getHeaderCount();
        $this->writeShort($headerCount);
        for ($i = 0; $i < $headerCount; ++$i) {
            $header = $message->headers[$i];
            $this->writeHeader($header);
        }
        // Write bodies
        $bodyCount = $message->getBodyCount();
        $this->writeShort($bodyCount);
        for ($i = 0; $i < $bodyCount; ++$i) {
            $body = $message->bodies[$i];
            $this->writeBody($body);
        }
        return $this->data;
    }

    /**
     * Serialize a message header
     *
     * @param MessageHeader $header
     */
    protected function writeHeader($header)
    {
        $this->writeUtf($header->name);
        $this->writeBoolean($header->mustUnderstand);
        $buffer = $this->data;
        $this->data = '';
        $this->run($header->data);
        $data = $this->data;
        $this->data = $buffer;
        $this->writeInt(strlen($data));
        $this->data .= $data;
    }

    /**
     * Serialize a message body
     *
     * @param MessageBody $body
     */
    protected function writeBody($body)
    {
        $this->writeUtf($body->targetURI);
        $this->writeUtf($body->responseURI);
        $buffer = $this->data;
        $this->data = '';
        $data = $this->run($body->data);
        $this->data = $buffer;
        $this->writeInt(strlen($data));
        $this->data .= $data;
    }

    /**
     * writeByte writes a single byte to the output stream
     * 0-255 range
     *
     * @param int $value An int that can be converted to a byte
     */
    public function writeByte($value)
    {
        $this->data .= pack('c', $value);
    }

    /**
     * writeBoolean writes the boolean code (0x01) and the data to the output stream
     *
     * @param bool $value The boolean value
     */
    public function writeBoolean($value)
    {
        $this->writeByte($value);
    }

    /**
     * writeShort takes an int and writes it as 2 bytes to the output stream
     * 0-65535 range
     *
     * @param int $value An integer to convert to a 2 byte binary string
     */
    public function writeShort($value)
    {
        $this->data .= pack('n', $value);
    }

    /**
     * writeInt takes an int, float, or double and converts it to a 4 byte binary string and
     * adds it to the output buffer
     *
     * @param long $value A long to convert to a 4 byte binary string
     */
    public function writeInt($value)
    {
        $this->data .= pack('N', $value);
    }

    /**
     * writeDouble takes a float as the input and writes it to the output stream.
     * Then if the system is little-endian, it reverses the bytes order because all
     * doubles passed via remoting are passed big-endian.
     *
     * @param double $value The double to add to the output buffer
     */
    public function writeDouble($value)
    {
        $b = pack('d', $value);
        $r = (!$this->bigEndianMachine) ? strrev($b) : $b;
        $this->data .= $r;
    }

    /**
     * writeUtf takes and input string, writes the length as an int and then
     * appends the string to the output buffer
     *
     * @param string $value The string less than 65535 characters to add to the stream
     */
    public function writeUtf($value)
    {
        $this->writeShort(strlen($value)); // write the string length - max 65535
        $this->data .= $value;
    }

    /**
     * writeLongUtf will write a string longer than 65535 characters.
     * It works exactly as writeUTF does except uses a long for the length
     * flag.
     *
     * @param string $value A string to add to the byte stream
     */
    protected function writeLongUtf($value)
    {
        $this->writeInt(strlen($value));
        $this->data .= $value;
    }

    public function curlSend($requestData, $endpoint = 'http://127.0.0.1/efxphp', $proxy = '127.0.0.1:8888', $proxyType = CURLPROXY_HTTP)
    {
        // Basic setup
        $curl = curl_init();
        curl_setopt($curl, CURLOPT_USERAGENT, 'EfxphpClient/php');
        curl_setopt($curl, CURLOPT_URL, $endpoint);

        if ($proxy != null) {
            curl_setopt($curl, CURLOPT_PROXY, $proxy);
            curl_setopt($curl, CURLOPT_PROXYTYPE, $proxyType);
        }

        // Headers
        $headers = array();
        $headers[] = 'Connection: Keep-Alive';
        $headers[] = 'Content-Type: application/x-amf';
        $headers[] = 'Content-length: ' . strlen($requestData);

        if (function_exists('gzdecode')) {
            if (function_exists('gzinflate')) {
                $headers[] = 'Accept-Encoding: gzip, deflate';
            } else {
                $headers[] = 'Accept-Encoding: gzip';
            }
        }

        curl_setopt($curl, CURLOPT_HTTPHEADER, $headers);
        curl_setopt($curl, CURLOPT_RETURNTRANSFER, false);
        curl_setopt($curl, CURLOPT_FOLLOWLOCATION, true);
        curl_setopt($curl, CURLOPT_SSL_VERIFYPEER, false);
        curl_setopt($curl, CURLOPT_SSL_VERIFYHOST, false);
        //curl_setopt($curl, CURLOPT_HEADERFUNCTION, array(&$this, 'responseHeaderCallback'));
        //curl_setopt($curl, CURLOPT_WRITEFUNCTION, array(&$this, 'responseWriteCallback'));
        curl_setopt($curl, CURLOPT_POST, 1);
        curl_setopt($curl, CURLOPT_POSTFIELDS, $requestData);

        // Execute, grab errors
        if (curl_exec($curl)) {
            //$this->response->status = curl_getinfo($curl, CURLINFO_HTTP_CODE);
        } else {
            //$this->response->error = new ResponseError(curl_error($curl), curl_errno($curl));
        }
        @curl_close($curl);
    }
}



