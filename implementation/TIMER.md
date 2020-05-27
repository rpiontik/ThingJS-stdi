# ThingJS timer interface

Provided timer functions.

## setInterval()
The setInterval() method calls a function at specified intervals (in milliseconds).
It method will continue calling the function until clearInterval() is called.

Syntax
```text
handle setInterval(function, milliseconds, param)
```
Example
```js
$res.timers.setInterval(function(param){
    print(param);
}, 3000, "Hello!");
```

## clearInterval()
The clearInterval() method clears a timer set with the setInterval() method. 

Syntax
```text
void clearInterval(handle)
```
Example
```js
let counter = 0;
let timer = $res.timers.setInterval(function(){
    print("Hello");
    counter++;
    if(counter > 10) clearInterval(timer);
}, 1000);
```

## setTimeout()
The setTimeout() method calls a function after a specified number of milliseconds.
The function is only executed once. If you need to repeat execution, use the setInterval() method.

Syntax
```text
handle setTimeout(function, milliseconds, param)
```
Example
```js
$res.timers.setTimeout(function(){
    print("Hello");
}, 1000);
```

## clearTimeout()
The clearTimeout() method clears a timer set with the setTimeout() method. 

Syntax
```text
void clearTimeout(handle)
```

Example
```js
let timer = $res.timers.setTimeout(function(){
    print("Hello");
}, 1000);
clearTimeout(timer);
```

# Manifest requires
```js
  "requires": {
    "interfaces": {
      "timers": { // Will available as $res.timers
        "type": "timers",
        "required": true / false,
        "description": { // Description for user
          "ru": "Таймеры",
          "en": "Timers"
        }
      }
    }
  }
```

#Files
1. tgsi_timer.h
2. tgsi_timer.c
2. TIMER.md

# Licensing

ThingsJS is released under
[GNU GPL v.2](http://www.gnu.org/licenses/old-licenses/gpl-2.0.html)
open source license.