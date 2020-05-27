# ThingJS GPIO input/output interface

Provide GPIO configuration functions.

## direction()
GPIO set direction. Configure GPIO direction.

Syntax
```text
void direction(const)
```
Available consts:
1. DIR_MODE_DISABLE - disable input and output
2. DIR_MODE_INPUT - input only
3. DIR_MODE_OUTPUT - output only mode
4. DIR_MODE_OUTPUT_OD - output only with open-drain mode
5. DIR_MODE_INPUT_OUTPUT_OD - output and input with open-drain mode
6. DIR_MODE_INPUT_OUTPUT - output and input mode

Example
```js
$res.blink.direction($res.blink.DIR_MODE_OUTPUT);
```

## set()
Set the RTC IO output level.

Syntax
```text
void set(bool)
```

Blink example
```js
let state = true;
$res.blink.direction($res.blink.DIR_MODE_OUTPUT);
$res.timers.setInterval(function () {
    $res.blink.set(state);
    state = !state;
}, 500);
```

## get()
GPIO get input level.

Syntax
```text
bool get(void)
```

Example
```js
$res.blink.direction($res.blink.DIR_MODE_INPUT);
$res.timers.setInterval(function () {
    if($res.blink.get()) {
        print("ON");    
    } else {
        print("OFF");    
    }   
}, 500);
```

## gpio
Property contain GPIO number.

Example
```js
print('Available GPIO', $res.blink.gpio);
```

# Manifest requires
```js
  "requires": {
    "interfaces": {
      "blink": { // Will be available as $res.blink
        "type": "bit_port", // Interface id
        "required": true / false,
        "description": { // Description for user
          "ru": "LED индикатор",
          "en": "LED indicator"
        }
      }
    }
  }
```

# Files
1. tgsi_bit_port.h
2. tgsi_bit_port.c
2. BIT_PORT.md


# Licensing

ThingsJS is released under
[GNU GPL v.2](http://www.gnu.org/licenses/old-licenses/gpl-2.0.html)
open source license.