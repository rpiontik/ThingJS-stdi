# ThingJS Smart LED controller interface

The Smart LED interface peripheral is primarily designed to control the intensity of LEDs,
although it can also be used to generate PWM signals for other purposes as well.
It has 16 channels which can generate independent waveforms that can be used, for example,
to drive RGB LED devices.

The interface based on [LEDC](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/ledc.html)
from Espressif

## reconfig()
Configure LEDC driver. 

Syntax
```text
void reconfig({
    int frequency;
    int resolution;
})
```

Example
```js
$res.ledc1.reconfig({
    resolution : 15,
    frequency : 2440 
});
```

## channels
Array of LEDC channels

### channels[].reconfig()
Configure LEDC channel. 

Syntax
```text
void channels[int].reconfig({
    bool inverse;
    int duty;
})
```

Example
```js
// Turn off all channels
for(let i = 0; i < $res.ledc1.channels.length; i++) {
    $res.ledc1.channels[i].reconfig({
        duty: 0,
        inverse: false
    });
}
```

### channels[].fade()
Run LEDC gradient from current duty to target duty.

Syntax
```text
void channels[int].fade(int target_duty, int time_interval)
```

Example
```js
// Turn off all channels
for(let i = 0; i < $res.ledc1.channels.length; i++) {
    $res.ledc1.channels[i].fade(
        0, // If not reverse will turned off channel
        0  // Apply immediately    
    );
}
// Smoothly turn on all channels within 10 seconds
for(let i = 0; i < $res.ledc1.channels.length; i++) {
    $res.ledc1.channels[i].fade(
        2440, // If not reverse will turned on channel
        10000 // Apply within 10 seconds    
    );
}
```

# Manifest requires
```js
  "requires": {
    "interfaces": {
      "ledc1": { // Will be available as $res.ledc1
        "type": "SmartLED", // Interface id
        "required": true / false,
        "description": {
          "ru": "Основной LEDC драйвер",
          "en": "Main LEDC driver"
        }
      },
      "ledc2": { // Will be available as $res.ledc2
        "type": "SmartLED", // Interface id
        "required": false / false,
        "description": {
          "ru": "Дополнительный LEDC драйвер",
          "en": "Extended LEDC driver"
        }
      }
    }
  }
```

# Sample application
[Lucerna](https://github.com/rpiontik/ThingJS-front/tree/master/src/applications/lucerna) LED Controller for aquarium 

# Files
1. tgsi_smart_led.h
2. tgsi_smart_led.c
2. SMART_LED.md


# Licensing

ThingsJS is released under
[GNU GPL v.2](http://www.gnu.org/licenses/old-licenses/gpl-2.0.html)
open source license.