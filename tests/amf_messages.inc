<?php
/**
 * efxphp (http://emilmalinov.com/efxphp)
 *
 * @copyright Copyright (c) 2015 Emil Malinov
 * @license   http://www.apache.org/licenses/LICENSE-2.0 Apache License 2.0
 * @link      http://github.com/emilkm/efxphp
 * @package   efxphp
 */

namespace flex\messaging\messages;

/**
 * @author     Emil Malinov
 * @package    efxphp
 * @subpackage amf
 */
class AbstractMessage
{
    /**
     * @var string
     */
    public $messageId;

    /**
     * @var string
     */
    public $clientId;

    /**
     * @var double
     */
    public $timestamp;

    /**
     * @var double
     */
    public $timeToLive;

    /**
     * @var string
     */
    public $destination;

    /**
     * @var string
     */
    public $headers;

    /**
     * @var string
     */
    public $body;

    /**
     * Generate a unique Id
     *
     * Format is: ########-####-####-####-############
     * Where # is an uppercase letter or number
     * example: 6D9DC7EC-A273-83A9-ABE3-00005FD752D6
     *
     * @return string
     */
    public function generateId()
    {
        return sprintf(
            '%08X-%04X-%04X-%02X%02X-%012X',
            mt_rand(),
            mt_rand(0, 65535),
            bindec(substr_replace(sprintf('%016b', mt_rand(0, 65535)), '0100', 11, 4)),
            bindec(substr_replace(sprintf('%08b', mt_rand(0, 255)), '01', 5, 2)),
            mt_rand(0, 255),
            mt_rand()
        );
    }

    /**
     * Validate an Id has the correct format
     *
     * @param mixed $value
     *
     * @return bool
     */
    public function validId($value)
    {
        return (preg_match("/^([0-9A-F]{8})-([0-9A-F]{4})-([0-9A-F]{4})-([0-9A-F]{4})-([0-9A-F]{12})$/i", $value)) ? true : false;
    }

    /**
     * The number of milliseconds elapsed
     * since the epoch of midnight on 1st Jan 1970
     *
     * @return double
     */
    public function timestampMilli()
    {
        return round(microtime(true) * 1000);
    }
}

class AsyncMessage extends AbstractMessage
{
    /**
     * @var string
     */
    public $correlationId;
}

class CommandMessage extends AsyncMessage
{
    const SUBSCRIBE_OPERATION = 0;
    const UNSUSBSCRIBE_OPERATION = 1;
    const POLL_OPERATION = 2;
    const CLIENT_SYNC_OPERATION = 4;
    const CLIENT_PING_OPERATION = 5;
    const CLUSTER_REQUEST_OPERATION = 7;
    const LOGIN_OPERATION = 8;
    const LOGOUT_OPERATION = 9;
    const SESSION_INVALIDATE_OPERATION = 10;
    const MULTI_SUBSCRIBE_OPERATION = 11;
    const DISCONNECT_OPERATION = 12;
    const UNKNOWN_OPERATION = 10000;

    public $operation = self::UNKNOWN_OPERATION;

    /**
     * Incoming: the deserializer will set all properties with values
     *           from the AMF packet.
     * Outgoing: constructor parameters must be provided.
     *
     * @param mixed $operation
     */
    public function __construct($operation = null)
    {
        if ($operation == null) {
            return;
        }

        $this->_explicitType = 'flex.messaging.messages.CommandMessage';

        if ($operation == self::CLIENT_PING_OPERATION) {
            $this->operation    = self::CLIENT_PING_OPERATION;
            $this->destination  = null;
            $this->messageId    = $this->generateId();
            $this->timestamp    = $this->timestampMilli();
            $this->timeToLive   = 0;
            $this->clientId     = null;
            $this->headers      = (object) array('DSId' => 'nil');
            $this->body         = (object) array();
        }
    }
}

class RemotingMessage extends AbstractMessage
{
    /**
     * The name of the service to be called including package name
     * @var String
     */
    public $source;

    /**
     * The name of the method to be called
     * @var string
     */
    public $operation;

    /**
     * Incoming: the deserializer will set all properties with values
     *           from the AMF packet.
     * Outgoing: constructor parameters must be provided.
     *
     * @param string $clientId
     * @param string $destination
     * @param string $source
     * @param string $operation
     * @param mixed  $params
     */
    public function __construct(
        $clientId = null,
        $destination = null,
        $source = null,
        $operation = null,
        $params = null
    ) {
        if ($clientId == null) {
            return;
        }

        $this->_explicitType = 'flex.messaging.messages.RemotingMessage';

        $this->clientId     = $clientId;
        $this->destination  = $destination;
        $this->messageId    = $this->generateId();
        $this->timestamp    = $this->timestampMilli();
        $this->timeToLive   = 0;
        $this->headers      = (object) array('DSId' => $this->clientId);
        $this->body         = $params;
        $this->source       = $source;
        $this->operation    = $operation;
    }
}

class AcknowledgeMessage extends AsyncMessage
{
    /**
     * Outgoing: the request message must be provided.
     * Incoming: the deserializer will set all properties with values
     *           from the AMF packet.
     *
     * @param CommandMessage|RemotingMessage|null $message
     */
    public function __construct($message = null)
    {
        if ($message == null) {
            return;
        }

        $this->_explicitType = 'flex.messaging.messages.AcknowledgeMessage';

        $this->destination  = null;
        $this->messageId    = $this->generateId();
        $this->timestamp    = $this->timestampMilli();
        $this->timeToLive   = 0;
        $this->body         = null;

        //set the client id
        if ($message->clientId != null) {
            $this->clientId = $message->clientId;
        } elseif (isset($message->headers->DSId)) {
            $this->clientId = $message->headers->DSId;
        }

        if ($this->clientId == null || $this->clientId == 'nil') {
            $this->clientId = $this->generateId();
        }

        //correlate the message
        if (isset($message->messageId)) {
            $this->correlationId = $message->messageId;
        }

        $this->headers      = (object) array('DSId' => $this->clientId);
    }
}

class ErrorMessage extends AcknowledgeMessage
{
    public $extendedData = null;
    public $faultCode;
    public $faultDetail;
    public $faultString = '';
    public $rootCause = null;

    /**
     * Outgoing: the request message must be provided.
     * Incoming: the deserializer will set all properties with values
     *           from the AMF packet.
     *
     * @param CommandMessage|RemotingMessage|null $message
     */
    public function __construct($message = null)
    {
        parent::__construct($message);

        if ($message == null) {
            return;
        }

        $this->_explicitType = 'flex.messaging.messages.ErrorMessage';
    }
}