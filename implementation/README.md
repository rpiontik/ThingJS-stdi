# ThingJS http interface

HTTP client for the ThingJS platform

## Features

- Supports GET, POST, PUT, PATCH, DELETE, HEAD, CONNECT, OPTIONS, TRACE and custom methods
- Supports Transfer-Encoding: chunked
- Supports timeout
- Supports simple auth
- Custom headers
- Production data function for long requests
- Processing data function for long responses
- Automatic transforms for JSON data

## Example

### Simple GET request
```js
$res.http.request('http://website.me/', function(response) {
    print(response.data);
});
```

### GET request with params
```js
$res.http.request({
    url: 'http://website.me/',
    params: {
        param1 : 'param1',
        param2 : 'param2'    
    }
}, function(response) {
    print(response.data);
});
```


### Simple JSON request
```js
$res.http.request({
    url: 'http://website.me/',
    method: $res.http.M_POST,
    content_type: $res.http.CT_JSON,
    data: {
        field1: 'field1',
        arr: ['item1', 'item2']    
    }   
}, function(response) {
    print(response.data);
});
```

### JSON chunked request
```js
$res.http.request({
    url: 'http://website.me/',
    method: $res.http.M_POST,
    content_type: $res.http.CT_JSON,
    transfer_encoding: $res.http.TE_CHUNKED,
    index: 0,
    data: function () {
        this.index++;
        if (this.index < 10) {
            return {index: this.index};
        } else {
            return;
        }
    }
}, function (response) {
    print(response.data);
});
```


# Licensing

ThingsJS is released under
[GNU GPL v.2](http://www.gnu.org/licenses/old-licenses/gpl-2.0.html)
open source license.




