# ThingJS Pulse counter interface

Provide PCNT configuration/operation functions.
For now operate with only one counter engine in simple way.
Counts positive fronts from 0 to 32767 

## getCount()
Get counter value

Syntax
```text
void getCount()
```
Example
```js
let pCnt = $res.PCNT.getCount();
```

## resetCounter()
Reset the counter to zero.

Syntax
```text
void resetCounter();
```

Reset counter example
```js
$res.PCNT.resetCounter();
```

## pcnt
Property contain GPIO number.

Example
```js
print('Counter GPIO', $res.PCNT.pcnt);
```

# Manifest requires
```js
  "requires": {
    "interfaces": {
      "PCNT": { // Will be available as $res.PCNT
        "type": "pcnt", // Interface id
        "required": true, // true / false,
        "default": 36, // default GPIO number
        "description": { // Description for user
          "ru": "Счетчик импульсов",
          "en": "Pulse counter"
        }
      }
    }
  }
```

# Files
1. tgsi_pcnt.h
2. tgsi_pcnt.c
2. PCNT.md

# Licensing

ThingsJS is released under
[GNU GPL v.2](http://www.gnu.org/licenses/old-licenses/gpl-2.0.html)
open source license.