# ThingJS MQTT client interface

MQTT client for the ThingJS platform.
Based on mqtt client by esp-idf.

## Features

- Supports MQTT over TCP, SSL with mbedtls, MQTT over Websocket, MQTT over Websocket Secure.
- Easy to setup with URI
- Multiple instances (Multiple clients in one application)
- Support subscribing, publishing, authentication, last will messages, keep alive pings and all 3 QoS levels (it should be a fully functional client).

## Configuration
* Curently support mqtt, mqtts, ws, wss schemes
  * MQTT over TCP samples:
    * ``mqtt://mqtt.eclipse.org``: MQTT over TCP, default port 1883:
    * ``mqtt://mqtt.eclipse.org:1884`` MQTT over TCP, port 1884:
    * ``mqtt://username:password@mqtt.eclipse.org:1884`` MQTT over TCP, port 1884, with username and password 
  * MQTT over SSL samples:
    * ``mqtts://mqtt.eclipse.org``: MQTT over SSL, port 8883
    * ``mqtts://mqtt.eclipse.org:8884``: MQTT over SSL, port 8884
  * MQTT over Websocket samples:
    * ``ws://mqtt.eclipse.org:80/mqtt``
  * MQTT over Websocket Secure samples:
    * ``wss://mqtt.eclipse.org:443/mqtt``
    
## Events
* **onbeforeconnected()** - The client is initialized and about to start connecting to the broker.        
* **onconnected()** - The client has successfully established a connection to the broker. The client is now ready to send and receive data.
* **ondisconnected()** - The client has aborted the connection due to being unable to read or write data, e.g. because the server is unavailable.
* **onsubscribed(msg_id)** - The broker has acknowledged the client’s subscribe request. The event data will contain the message ID of the subscribe message.
* **onunsubscribed(msg_id)** - The broker has acknowledged the client’s unsubscribe request. The event data will contain the message ID of the unsubscribe message.
* **onpublished(msg_id)** - The broker has acknowledged the client’s publish message. This will only be posted for Quality of Service level 1 and 2, as level 0 does not use acknowledgements. The event data will contain the message ID of the publish message.
* **ondata(topic, data)** - The client has received a publish message. The event data contains: message ID, name of the topic it was published to, received data and its length. For data that exceeds the internal buffer multiple ondata will be posted.
* **onerror(err_code, errtls_code)** -   The client has encountered an error. Event data can be used to further determine the type of the error.

Example

```javascript
$res.mqtt.onconnected = function () {
    print('MQTT is connected');
    $res.mqtt.mqtt.subscribe('home/room1/temp');
};

$res.mqtt.ondata = function (topic, data) {
    print('Current temp is', data);
};
```


## Functions
### connect(URI)
Starts mqtt client with URI.

Syntax
 
``void connect(string)``

Example

```javascript
$res.mqtt.connect('mqtts://mqtt.eclipse.org');
```

### reconnect() 
Used to force reconnection.

Syntax

``void reconnect(void)``

Example

```javascript
$res.mqtt.reconnect();
```

### disconnect() 
Used to force disconnection from the broker.

Syntax

``void disconnect(void)``

Example

```javascript
$res.mqtt.disconnect();
```


### subscribe(topic, qos) 
Subscribe the client to defined topic with defined qos.

Syntax

``int subscribe(string, int)``

Notes:
* Client must be connected to send subscribe message
* This API is could be executed from a user task or from a mqtt event callback i.e. internal mqtt task (API is protected by internal mutex, so it might block if a longer data receive operation is in progress.

Return:
* msg_id of the subscribe message on success -1 on failure

Example

```javascript
let msgid = $res.mqtt.subscribe('home/room1/temp');
```

### ``msg_id unsubscribe(topic)`` 
Unsubscribe the client from defined topic.

Syntax

``int unsubscribe(string)``

Notes:
* Client must be connected to send unsubscribe message
* It is thread safe, please refer to subscribe for details

Return:
* msg_id of the subscribe message on success -1 on failure

Example

```javascript
let msgid = $res.mqtt.unsubscribe('home/room1/temp');
```

### ``msg_id publish(topic, data, qos, retain)`` 
Client to send a publish message to the broker.

Syntax

``int publish(string, string, [int], [int])``

Notes:
* This API might block for several seconds, either due to network timeout (10s) or if publishing payloads longer than internal buffer (due to message fragmentation)
* Client doesn’t have to be connected to send publish message (although it would drop all qos=0 messages, qos>1 messages would be enqueued)
* It is thread safe, please refer to subscribe for details

Return:
* msg_id of the publish message (for QoS 0 message_id will always be zero) on success. -1 on failure.

Example

```javascript
let msgid = $res.mqtt.publish('home/room1/temp', 33.2);
```

# Files
1. tgsi_mqqtc.h
2. tgsi_mqqtc.c
2. MQTT_CLIENT.md

# Licensing
ThingsJS is released under
[GNU GPL v.2](http://www.gnu.org/licenses/old-licenses/gpl-2.0.html)
open source license.