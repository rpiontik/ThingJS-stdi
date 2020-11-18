# ThingJS internal clock interface

Provide system information.

## idf_version
The idf version of firmware. 

Syntax
```text
string idf_version
```

Example
```js
print($res.sys_info.idf_version);
```

## chip_id
The unique chip ID.

Syntax
```text
string chip_id
```

Example
```js
print($res.sys_info.chip_id);
```

# Files
1. tgsi_sys_info.h
2. tgsi_sys_info.c
2. SYS_INFO.md

# Sample application
[Lucerna](https://github.com/rpiontik/ThingJS-front/tree/master/src/applications/lucerna) LED Controller for aquarium 

# Licensing

ThingsJS is released under
[GNU GPL v.2](http://www.gnu.org/licenses/old-licenses/gpl-2.0.html)
open source license.