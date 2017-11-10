<?php

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


/******************************************************************************/
/*                                  AMF                                       */
/******************************************************************************/

class ActionMessage
{
    /**
     * @var int
     */
    public $version;

    /**
     * @var array
     */
    public $headers;

    /**
     * @var array
     */
    public $bodies;

    /**
     * @param int $version
     */
    public function __construct($version)
    {
        $this->version = $version;
        $this->bodies = array();
    }

    /**
     * @return int The number of headers
     */
    public function getHeaderCount()
    {
        return (is_array($this->headers)) ? count($this->headers) : 0;
    }

    /**
     * @return int The number of bodies
     */
    public function getBodyCount()
    {
        return (is_array($this->bodies)) ? count($this->bodies) : 0;
    }

    /**
     * @param mixed $index
     *
     * @return MessageBody
     */
    public function getBody($index)
    {
        return $this->bodies[$index];
    }
}

class MessageHeader
{
    /**
     * @var string
     */
    public $name;

    /**
     * @var bool
     */
    public $mustUnderstand;

    /**
     * @var mixed
     */
    public $data;

    /**
     * @param string $name
     * @param bool $mustUnderstand
     * @param mixed $data
     */
    public function __construct($name = '', $mustUnderstand = false, $data = null) {
        $this->name = $name;
        $this->mustUnderstand = (bool) $mustUnderstand;
        $this->data = $data;
    }
}

class MessageBody
{
    /**
     * @var string
     */
    public $targetURI;

    /**
     * @var string
     */
    public $responseURI;

    /**
     * @var mixed
     */
    public $data;

    /**
     * Incoming: the deserializer will set all properties with values
     * from the AMF packet. Outgoing: parameters must be provided.
     *
     * @param string $targetURI
     * @param string $responseURI
     */
    public function __construct($targetURI = null, $responseURI = null)
    {
        if ($targetURI == null && $responseURI == null) {
            return;
        }

        $this->_explicitType = 'flex.messaging.io.amf.MessageBody';

        $this->targetURI = $targetURI == null ? 'null' : $targetURI;
        $this->responseURI = $responseURI == null ? 'null' : $responseURI;
    }

    /**
     * @return AbstractMessage
     */
    public function getDataAsMessage()
    {
        if (is_array($this->data)) {
            $message = $this->data[0];
        } else {
            $message = $this->data;
        }
        if (!($message instanceof CommandMessage)
            && !($message instanceof RemotingMessage)
            && !($message instanceof AcknowledgeMessage)
            && !($message instanceof ErrorMessage)
        ) {
            throw new Exception('Unsupported message type.');
        }
        return $message;
    }
}

class Request
{
    public $source;
    public $operation;
    public $params;

    /**
     * @param string   $source
     * @param stirng   $operation
     * @param mixed    $params
     */
    public function __construct($source, $operation, $params)
    {
        $this->source = $source;
        $this->operation = $operation;
        $this->params = $params;
    }
}

class Response
{
    /**
     * @var string
     */
    public $_explicitType = 'emilkm.efxphp.Response';

    /**
     * @var int
     */
    public $code = 0;

    /**
     * @var string
     */
    public $message;

    /**
     * @var mixed
     */
    public $detail;

    /**
     * @var mixed
     */
    public $data;

    /**
     * Incoming: the deserializer will set all properties with values
     *           from the AMF packet.
     *
     * @param mixed $data
     * @param mixed $detail
     */
    public function __construct($data = null, $detail = null)
    {
        $this->data = $data;
        $this->detail = $detail;
    }
}

class ResponseError
{
    /**
     * @var string
     */
    public $_explicitType = 'emilkm.efxphp.ResponseError';

    /**
     * @var int
     */
    public $code = 0;

    /**
     * @var string
     */
    public $message;

    /**
     * @var mixed
     */
    public $detail;

    /**
     * @var mixed
     */
    public $data;

    /**
     * @param mixed $message
     * @param mixed $code
     * @param mixed $data
     * @param mixed $detail
     */
    public function __construct($message, $code = 0, $detail = null, $data = null)
    {
        $this->message = $message;
        $this->code = $code;
        $this->detail = $detail;
        $this->data = $data;
    }
}