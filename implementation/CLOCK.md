# ThingJS internal clock interface

Provide internal clock functions.

## setTime()
Epoch (unix time) that specifies the time that you want to set. Timezone must be included. 

Syntax
```text
void setTime(epoch)
```

Example
```js
$res.clock.setTime(1590562990); // Wednesday, 27 May 2020 г., 7:03:10
```

## getTime()
This function returns the epoch time. Timezone included.

Syntax
```text
epoch getTime(void)
```

Example
```js
$res.timers.setInterval(function () {
    print($res.clock.getTime());
}, 1000);
```

# Manifest requires
```js
  "requires": {
    "interfaces": {
      "clock": { // Will available as $res.clock
        "type": "clock", // Interface id
        "required": true / false,
        "description": {
          "ru": "Системные часы",
          "en": "System clock"
        }
      }
    }
  }
```

# Files
1. tgsi_clock.h
2. tgsi_clock.c
2. BIT_CLOCK.md


# Licensing

ThingsJS is released under
[GNU GPL v.2](http://www.gnu.org/licenses/old-licenses/gpl-2.0.html)
open source license.