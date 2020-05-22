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

## Request Config
These are the available config options for making requests. Only the `url` is required. Requests will default to `GET` if `method` is not specified.

```js
{
  // `url` is the server URL that will be used for the request
  url: 'https://some-domain.com/api/user',

  // `method` is the request method to be used when making the request
  // Available: M_GET, M_HEAD, M_POST, M_PUT, M_DELETE, M_CONNECT, M_OPTIONS, M_TRACE, M_PATCH
  // and custom methods  
  method: M_GET, // default

  // `headers` are custom headers to be sent
  headers: {'X-Requested-With': 'XMLHttpRequest'},

  // `params` are the URL parameters to be sent with the request
  // Must be a plain object or a URLSearchParams object
  params: {
    ID: 12345
  },

  // `data` is the data to be sent as the request body
  // Only applicable for request methods 'PUT', 'POST', 'PATCH' and custom
  // When no `transformRequest` is set, must be of one of the following types:
  // - string, plain object, ArrayBuffer, ArrayBufferView, URLSearchParams
  // - Browser only: FormData, File, Blob
  // - Node only: Stream, Buffer
  data: {
    firstName: 'Fred'
  },
  
  // syntax alternative to send data into the body
  // method post
  // only the value is sent, not the key
  data: 'Country=Brasil&City=Belo Horizonte',

  // `timeout` specifies the number of milliseconds before the request times out.
  // If the request takes longer than `timeout`, the request will be aborted.
  timeout: 1000, // default is `1000` (no timeout)

  // `auth` indicates that HTTP Basic auth should be used, and supplies credentials.
  // This will set an `Authorization` header, overwriting any existing
  // `Authorization` custom headers you have set using `headers`.
  // Please note that only HTTP Basic auth is configurable through this parameter.
  // For Bearer tokens and such, use `Authorization` custom headers instead.
  auth: {
    username: 'user',
    password: 'password'
  },

  // Allows handling of response events
  onResponseProcess: function (response) {
  }

  // Allows handling of networks error events
  onError: function (error) {
  }

}
```

## Example

### Simple GET request
```js
$res.http.request('http://website.me/', function(response) {
    print('status:', response.code);
    print('data:', response.data);
});
```

### Request with headers
```js
$res.http.request({
    url: 'http://website.me/',
    headers: {
        header1 : 'header1',
        header2 : 'header2'    
    }
}, function(response) {
    print('headers:');
    for (let header in response.headers) {
        print('   [', header, ']: [', response.headers[header], ']');
    }
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

### Simple JSON request and response
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
    print(JSON.stringify(response.data));
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




