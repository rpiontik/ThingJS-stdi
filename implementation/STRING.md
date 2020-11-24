# ThingJS string library

Provide string functions.

## toString()
The method returns a string representing the specified object. 

Syntax
```text
toString(any)
```

Example
```js
$res.string.toString(123); //"123" 
```

## split()
The method divides a String into an ordered list of substrings, puts these substrings into an array, 
and returns the array.  

Syntax
```text
split(any,[separator[, limit]])
```

Example
```js
$res.string.split(123, 2);              //["1","3"] 
$res.string.split("fo|go|do", "|", 2);  //["fo","go"]
$res.string.split(123, "");             //["","1","2","3"]
```

## replaceAll()
The method returns a new string with all matches of a string replaced by a replacement. 

Syntax
```text
replaceAll(any, substr, newSubstr)
```

Example
```js
$res.string.replaceAll(123, 1, "test|");    //"test|23" 
$res.string.replaceAll(123, "", "|");       //"|1|2|3"
$res.string.replaceAll(123, 2, "");         //"13"
```

## template()
Template literals are string literals allowing embedded expressions. 
These are indicated by the double curly braces ({{expression}}).

Syntax
```text
template(template)
```

Example
```js
$res.string.template("1 + 1 = {{1+1}}");    //"1 + 1 = 2"
let x = 10; 
$res.string.template("5 + 5 = {{x}}");      //"5 + 5 = 10"
```

## mustache()
Logic-less templates. Based on [mustache project](https://mustache.github.io/).

Not supported:
* {{{ }}}
* <%= =%>

Syntax
```text
mustache(template, context)
```

Example
```js
let context = {
                "name": "Chris",
                "value": 10000,
                "taxed_value": 10000 - (10000 * 0.4),
                "in_ca": true
              };
$res.string.mustache("Hello {{name}}\n"
                      +"You have just won {{value}} dollars!\n"
                      +"{{#in_ca}}\n"
                      +"Well, {{taxed_value}} dollars, after taxes.\n"
                      +"{{/in_ca}}",
                      context
                    );    
//Hello Chris
//You have just won 10000 dollars!
//Well, 6000.0 dollars, after taxes.
```
[More examples](http://mustache.github.io/mustache.5.html) 


# Files
1. tgsi_string.h
2. tgsi_string.c
2. BIT_STRING.md

# Licensing

ThingsJS is released under
[GNU GPL v.2](http://www.gnu.org/licenses/old-licenses/gpl-2.0.html)
open source license.