# Overview
The repository is module of ThingJS (open source IoT platform). 
You can create custom interface too. See the repository as example.

# Install
git submodule add git@github.com:rpiontik/ThingJS-stdi.git components/thingjs_stdi

# Requirements
* ESP-IDF version: release/v4.0
* ThingJS-boards (https://github.com/rpiontik/ThingsJS-boards)
* ThingJS-extern (https://github.com/rpiontik/ThingJS-extern)
* ThingJS-core (https://github.com/rpiontik/ThingJS-core)

# Package includes
1. [clock](/implementation/CLOCK.md) - Provide internal clock functions;
2. [bit_port](/implementation/BIT_PORT.md) - Provide GPIO configuration functions;
3. [HTTP](/implementation/HTTP.md) - HTTP/HTTPS client;
4. [DS3231](/implementation/DS3231.md) - Provide RTC functions for chip ds3231 and compatible;
5. [SmartLED](/implementation/SMART_LED.md) - Smart LED interface;
6. [DS18X20](/implementation/DS18X20.md) - DS18X20 interface;
7. [mqttc](/implementation/MQTT_CLIENT.md) - MQTT client.
8. [sys_info](/implementation/SYS_INFO.md) - System information.

# Thankful
* [JetBrains](https://www.jetbrains.com/) for OpenSource license.

# Licensing
ThingsJS is released under
[GNU GPL v.2](http://www.gnu.org/licenses/old-licenses/gpl-2.0.html)
open source license.




