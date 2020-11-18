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

## replaceAll()
The method returns a new string with all matches of a string replaced by a replacement. 

Syntax
```text
replaceAll(any, substr, newSubstr)
```

Example
```js
$res.string.replaceAll(123, 1, "test|"); //"test|23" 
$res.string.replaceAll(123, "", "|"); //"|1|2|3"
$res.string.replaceAll(123, 2, ""); //"13"
```

# Files
1. tgsi_string.h
2. tgsi_string.c
2. BIT_STRING.md

# Licensing

ThingsJS is released under
[GNU GPL v.2](http://www.gnu.org/licenses/old-licenses/gpl-2.0.html)
open source license.