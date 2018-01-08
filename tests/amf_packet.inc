<?php

namespace emilkm\efxphp\Amf;

use Exception;
use stdClass;

class Constants
{
    //flex.messaging.io.MessageIOConstants
    const REMOTE_CLASS_FIELD        = '_explicitType';
    const EXTERNALIZED_DATA_FIELD   = '_externalizedData';

    const RESULT_METHOD             = '/onResult';
    const STATUS_METHOD             = '/onStatus';

    const EMPTY_STRING              = '';
    const NULL_STRING               = 'null';

    const AMF0_OBJECT_ENCODING      = 0;

    const AMF0_NUMBER               = 0;
    const AMF0_BOOLEAN              = 1;
    const AMF0_STRING               = 2;
    const AMF0_OBJECT               = 3;
    const AMF0_MOVIECLIP            = 4;
    const AMF0_NULL                 = 5;
    const AMF0_UNDEFINED            = 6;
    const AMF0_REFERENCE            = 7;
    const AMF0_MIXEDARRAY           = 8; //ECMAArray
    const AMF0_OBJECTEND            = 9;
    const AMF0_ARRAY                = 10; //StrictArray
    const AMF0_DATE                 = 11;
    const AMF0_LONGSTRING           = 12;
    const AMF0_UNSUPPORTED          = 13;
    const AMF0_RECORDSET            = 14;
    const AMF0_XMLDOCUMENT          = 15;
    const AMF0_TYPEDOBJECT          = 16;
    const AMF0_AMF3                 = 17;

    const AMF3_OBJECT_ENCODING      = 3;

    const AMF3_UNDEFINED            = 0;
    const AMF3_NULL                 = 1;
    const AMF3_BOOLEAN_FALSE        = 2;
    const AMF3_BOOLEAN_TRUE         = 3;
    const AMF3_INTEGER              = 4;
    const AMF3_DOUBLE               = 5;
    const AMF3_STRING               = 6;
    const AMF3_XMLDOCUMENT          = 7;
    const AMF3_DATE                 = 8;
    const AMF3_ARRAY                = 9;
    const AMF3_OBJECT               = 10;
    const AMF3_XML                  = 11;
    const AMF3_BYTEARRAY            = 12;
    const AMF3_VECTOR_INT           = 13;
    const AMF3_VECTOR_UINT          = 14;
    const AMF3_VECTOR_DOUBLE        = 15;
    const AMF3_VECTOR_OBJECT        = 16;
    const AMF3_DICTIONARY           = 17;

    // Object encodings for AMF3 object types
    const ET_PROPLIST               = 0;
    const ET_EXTERNAL               = 1;
    const ET_DYNAMIC                = 2;
    const ET_PROXY                  = 3;

    const FMS_OBJECT_ENCODING       = 1;

    const UNKNOWN_CONTENT_LENGTH    = 1;

    const AMF_U8_MAX                = 255;
    const AMF_U16_MAX               = 65535;
    const AMF_U32_MAX               = 4294967295;
    const AMF3_INT28_MAX            = 268435455;
    const AMF3_INT28_MIN            = -268435456;
    const AMF3_UINT29_MAX           = 536870911;

    const MAX_STORED_OBJECTS        = 1024;
}


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

    /*public function sessionId() {
        foreach ($this->headers as $header) {
            if ($header->name == 'sid') {
                return $header->data;
            }
        }
        return null;
    }*/
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

        $remoteClassField = Constants::REMOTE_CLASS_FIELD;
        $this->$remoteClassField = 'flex.messaging.io.amf.MessageBody';

        $this->targetURI = $targetURI;
        $this->responseURI = $responseURI;
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

