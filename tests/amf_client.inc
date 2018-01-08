<?php
require_once __DIR__ . '/amf_encoder.inc';
require_once __DIR__ . '/amf_decoder.inc';
require_once __DIR__ . '/amf_classes.inc';
require_once __DIR__ . '/amf_messages.inc';

use Exception;

class Client
{
    const REQUEST_TIMEOUT = 30000; //30 seconds

    /**
     * Proxy to send requests through
     * @var mixed
     */
    private $proxy = null;

    /**
     * @var Deserializer
     */
    private $deserializer = null;

    /**
     * @var Serializer
     */
    private $serializer = null;

    /**
     * @var Request
     */
    private $request = null;

    /**
     * @var Response
     */
    private $response = null;

    private $sidPropagation = 'header'; // header or query

    private $sequence = 0;

    public $clientId = null;
    public $sessionId = null;
    public $remoteLogin = null;

    public $requestTimeout = self::REQUEST_TIMEOUT;

    public $destination = '';
    public $endpoint = '';
    public $headers = array();


    /**
     * @param Deserializer $deserializer
     * @param Serializer   $serializer
     * @param string       $destination
     * @param string       $endpoint
     * @param int          $requestTimeout
     */
    public function __construct(
        Deserializer $deserializer,
        Serializer $serializer,
        $destination,
        $endpoint,
        $requestTimeout = self::REQUEST_TIMEOUT
    ) {
        $this->deserializer = $deserializer;
        $this->serializer = $serializer;
        $this->destination = $destination;
        $this->endpoint = $endpoint;
        $this->requestTimeout = $requestTimeout;
    }

    /**
     * Set (debug) proxy information. To unset proxy information
     * call without parameters.
     *
     * @param string   $host Proxy hostname and port (localhost:8888)
     * @param constant $type CURL proxy type
     */
    public function setProxy($host = null, $type = CURLPROXY_HTTP)
    {
        if ($host == null) {
            $this->proxy = null;
        } else {
            $this->proxy = (object) array('host' => $host, 'type' => $type);
        }
    }

    /**
     * 'header' = through AMF RemoteMessage header sID, 'query' = through query string sID
     *
     * @param string $value
     */
    public function setSidPropagation($value)
    {
        if ($value != 'header' && $value != 'query') {
            throw new Exception("sidPropagation: 'header' = through AMF RemoteMessage header sID, 'query' = through query string sID");
        }
        $this->sidPropagation = $value;
    }

    /**
     * Set the session id for all further requests to the server.
     *
     * @param string $sessionId
     * @param bool   $releaseQueue
     */
    public function setSessionId($sessionId)
    {
        $this->sessionId = $sessionId;

        if ($this->sidPropagation == 'header')  {
            $this->headers['sID'] = $this->sessionId;
        } else {
            $this->endpoint .= '?sID=' . $this->sessionId;
        }
    }

    /**
     * @param callable $onResult
     * @param callable $onStatus
     */
    public function ping()
    {
        if ($this->clientId == null && $this->sequence == 0) {
            $pingRequest = new Request('command', 'ping', array());
            return $this->send($pingRequest);
        }
    }

    /**
     * @param string   $source
     * @param string   $operation
     * @param array    $params
     * @param callable $onResult
     * @param callable $onStatus
     * @param mixed    $token
     * @param bool     $holdQueue
     */
    public function invoke($source, $operation, $params)
    {
        $params = $this->safeParams($params);
        if ($this->clientId == null && $this->sequence == 0) {
            $response = $this->ping();
            if ($response instanceof ResponseError) {
                return $response;
            }
        }
        $request = new Request($source, $operation, $params);
        return $this->send($request);
    }

    private function createMessage(Request $request)
    {
        $actionMessage = new ActionMessage(3);
        $messageBody = new MessageBody();
        $message;
        if ($this->sequence == 0) {
            $this->sequence++;
            $messageBody->targetURI = 'null';
            $messageBody->responseURI = '/' . $this->sequence;
            $message = new CommandMessage(CommandMessage::CLIENT_PING_OPERATION);
            $message->destination = $this->destination;
        } else {
            $this->sequence++;
            $messageBody->targetURI = 'null';
            $messageBody->responseURI = '/' . $this->sequence;
            $message = new RemotingMessage(
                $this->clientId,
                $this->destination,
                $request->source,
                $request->operation,
                $request->params
            );

            foreach ($this->headers as $key => $value) {
                $message->headers->$key = $value;
            }
        }

        $messageBody->data[0] = $message;
        $actionMessage->bodies[] = $messageBody;
        return $actionMessage;
    }

    private function send(Request $request)
    {
        $outMessage = $this->createMessage($request);
        $requestData = $this->serializer->writeMessage($outMessage);

        $this->response = (object) array(
            'status' => '',
            'error' => false,
            'headers' => array(),
            'data' => null
        );

        // Basic setup
        $curl = curl_init();
        curl_setopt($curl, CURLOPT_USERAGENT, 'EfxphpClient/php');
        curl_setopt($curl, CURLOPT_URL, $this->endpoint);

        if ($this->proxy != null) {
            curl_setopt($curl, CURLOPT_PROXY, $this->proxy->host);
            curl_setopt($curl, CURLOPT_PROXYTYPE, $this->proxy->type);
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
        curl_setopt($curl, CURLOPT_HEADERFUNCTION, array(&$this, 'responseHeaderCallback'));
        curl_setopt($curl, CURLOPT_WRITEFUNCTION, array(&$this, 'responseWriteCallback'));
        curl_setopt($curl, CURLOPT_POST, 1);
        curl_setopt($curl, CURLOPT_POSTFIELDS, $requestData);

        // Execute, grab errors
        if (curl_exec($curl)) {
            $this->response->status = curl_getinfo($curl, CURLINFO_HTTP_CODE);
        } else {
            $this->response->error = new ResponseError(curl_error($curl), curl_errno($curl));
        }
        @curl_close($curl);


        if ($this->response->error !== false) {
            return $this->response->error;
        } elseif ($this->response->status != 200) {
            $this->response->error = new ResponseError('HTTP status not 200.');
            return $this->response->error;
        } elseif (!isset($this->response->headers['content-type'])
            || (isset($this->response->headers['content-type']) && strpos($this->response->headers['content-type'], 'application/x-amf') === false)
        ) {
            $this->response->error = new ResponseError('Unsupported content type.', 1, $this->response->headers['content-type']);
            return $this->response->error;
        }

        if (isset($this->response->headers['content-encoding'])) {
            if (strtolower($this->response->headers['content-encoding']) == 'gzip') {
                if (!function_exists('gzdecode')) {
                    $this->response->error = new ResponseError('Cannot decode the response data.', 1, 'gzdecode not available.');
                    return $this->response->error;
                }
                if (!($decoded = @gzdecode($this->response->data))) {
                    $this->response->error = new ResponseError('Failed to decode the response data.', 1, 'gzdecode failed.');
                    return $this->response->error;
                }
            } elseif (strtolower($this->response->headers['content-encoding']) == 'deflate') {
                if (!function_exists('gzinflate')) {
                    $this->response->error = new ResponseError('Cannot decode the response data.', 1, 'gzinflate not available.');
                    return $this->response->error;
                }
                if (!($decoded = @gzinflate($this->response->data))) {
                    $this->response->error = new ResponseError('Failed to decode the response data.', 1, 'gzinflate failed.');
                    return $this->response->error;
                }
            }
            $this->response->data = $decoded;
        }

        try {
            $inMessage = $this->deserializer->readMessage($this->response->data);
        } catch (Exception $e) {
            $this->response->error = new ResponseError('Failed to deserialize response data.', 2, $e->getMessage());
            return $this->response->error;
        }

        $bodyCount = $inMessage->getBodyCount();

        if ($bodyCount == 0) {
            $this->response->error = new ResponseError('Malformed AMF packet.', 3, 'Missing AMF body.');
            return $this->response->error;
        }

        $responseBody = $inMessage->getBody(0);

        try {
            $responseMessage = $responseBody->getDataAsMessage();
        } catch (Exception $e) {
            $this->response->error = new ResponseError('Malformed AMF packet.', 4, 'Unsupported AMF message type.');
            return $this->response->error;
        }

        if (!($responseMessage instanceof AcknowledgeMessage) && !($responseMessage instanceof ErrorMessage)) {
            $this->response->error = new ResponseError('Malformed AMF packet.', 5, 'Unexpected AMF message type.');
            return $this->response->error;
        }

        if ($responseMessage instanceof ErrorMessage) {
            $this->response->error = new ResponseError($responseMessage->faultString, $responseMessage->faultCode, $responseMessage->faultDetail);
            return $this->response->error;
        }

        if ($responseBody->targetURI == '/1/onResult') {
            $this->clientId = $responseMessage->clientId;
            return $responseMessage->body;
        } elseif ($responseMessage->body instanceof Response && $responseMessage->body->code < 0) {
            $this->response->error = new ResponseError($responseMessage->body->message, $responseMessage->body->code, $responseMessage->body->detail);
            return $this->response->error;
        } elseif (isset($responseMessage->body->type) && $responseMessage->body->type == -1) {
            $this->response->error = new ResponseError($responseMessage->body->message, $responseMessage->body->code, $responseMessage->body->detail);
            return $this->response->error;
        } else {
            return $responseMessage->body;
        }
    }

    private function safeParams($params)
    {
        if (is_null($params)) {
            $params = array();
        } elseif (!is_array($params)) {
            $params = array($params);
        }
        return $params;
    }

    /**
     * CURL header callback
     *
     * @param resource $curl CURL resource
     * @param string $data Data
     * @return integer
     */
    private function responseHeaderCallback($curl, $data)
    {
        if (($strlen = strlen($data)) <= 2) {
            return $strlen;
        }
        if (substr($data, 0, 4) == 'HTTP') {
            $this->response->status = (int) substr($data, 9, 3);
        } else {
            $data = trim($data);
            if (strpos($data, ': ') === false) {
                return $strlen;
            }
            list($header, $value) = explode(': ', $data, 2);
            $this->response->headers[strtolower($header)] = $value;
        }
        return $strlen;
    }

    /**
     * CURL write callback
     *
     * @param resource &$curl CURL resource
     * @param string &$data Data
     * @return integer
     */
    private function responseWriteCallback($curl, $data)
    {
        $this->response->data = $data;
        return strlen($data);
    }
}
