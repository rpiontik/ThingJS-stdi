# ThingJS http interface

HTTP/HTTPS client for the ThingJS platform.
Based in axios specification.

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
  
  // Set true for trace traffic to stdout
  debug: true,

  // `headers` are custom headers to be sent
  headers: {'X-Requested-With': 'ThingsJS'},

  // Available: CT_JSON, CT_TEXT, CT_FORM_URLENCODED, CT_MULTIPART_FORM_DATA, string (custom)
  content_type: CT_FORM_URLENCODED, // default

  // Available: TE_NONE, TE_CHUNKED, string (custom)
  transfer_encoding: TE_NONE, // default

  // `params` are the URL parameters to be sent with the request
  // Must be a plain object or a URLSearchParams object
  params: {
    ID: 12345
  },

  // `data` is the data to be sent as the request body
  // Only applicable for request methods 'PUT', 'POST', 'PATCH' and custom
  // Must be of one of the following types:
  // - string, object
  // - function
  data: {
    firstName: 'Fred'
  },
  
  // syntax alternative to send data into the body
  // method post
  data: 'Country=Brasil&City=Belo Horizonte',

  // also alternative to send data into the body by function
  // method post
  data: function() {
    return 'Country=Brasil&City=Belo Horizonte';
  },

  // Complex multipart/form-data
  // method post
  data: {
    file1: {
        headers: {
            'Content-Disposition:': 'form-data; name="file"; filename="sample.txt"',
            'Content-Type': 'text/plain'
        },
        // Simple data 
        data: 'File content'
        // also alternative to send data by function
        data: function() {
            return 'File content';
        }          
    }
  },

  // `timeout` specifies the number of milliseconds before the request times out.
  // If the request takes longer than `timeout`, the request will be aborted.
  timeout: 1000, // default is `1000` (no timeout)

  // `auth` indicates that HTTP Basic auth should be used, and supplies credentials.
  auth: {
    username: 'user',
    password: 'password'
  },

  // Allows handling of response events
  // The function is called as long as there is data in the stream.
  // You can break process by return a false
  // response.headers   - response headers
  // response.data      - part of stream
  // response.code      - status code
  //           
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
$res.http.request('http://website.me/',
    //onResponseProcess
    function (response) {
        if (response.data) {
            this.buffer = this.buffer ? (this.buffer + response.data) : response.data;
        } else {
            print('status:', response.code);
            print('response: [', this.buffer, ']');
        }
    }
);
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
    // Break response processing
    return false;
});
```

### GET request with params
```js
$res.http.request({
    url: 'http://website.me/',
    params: {
        param1 : 'param1',
        param2 : 'param2'    
    },
    onResponseProcess: function(response) {
        if (response.data) {
            this.buffer = this.buffer ? (this.buffer + response.data) : response.data;
        } else {
            print('status:', response.code);
            print('response: [', this.buffer, ']');
        }
    }
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

### application/x-www-form-urlencoded request
```js
$res.http.request({
    url: 'http://website.me/',
    method: $res.http.M_POST,
    content_type: $res.http.CT_FORM_URLENCODED,
    data: {
        field1: 'field1',
        field2: 'field2'
    }   
}, function(response) {
    print(response.code);
    return false;
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
    print('status:', response.code);
    return false;
});
```

### Complex multipart/form-data request
```js
$res.http.request({
    url: 'http://website.me/',
    method: $res.http.M_POST,
    content_type: $res.http.CT_MULTIPART_FORM_DATA,
    transfer_encoding: $res.http.TE_CHUNKED,
    index1: 0,
    index2: 0,
    data: {
        // Simple value
        field1: 'Text1',
        // Production function
        field2: function() {
            this.index1++;
            if(this.index1 < 10)
                return '|text2|'
            else
                return;
        },
        // Custom headers for part
        short_file_txt: {
            headers: {
                'Content-Disposition:': 'form-data; name="file"; filename="sample1.txt"',
                'Content-Type': 'text/plain'
            },
            data: 'Simple text file'           
        },
        // Custom headers for part and Production function
        long_field_text: {
            headers: {
                'Content-Disposition:': 'form-data; name="file"; filename="sample2.txt"',
                'Content-Type': 'text/plain'
            },
            data: function() {
                this.index2++;
                if(this.index2 < 100)
                    return '|long text file|'
                else
                    return;
            }                      
         }
    }   
}, function(response) {
    print(response.code);
    return false;
});
```

# Licensing

ThingsJS is released under
[GNU GPL v.2](http://www.gnu.org/licenses/old-licenses/gpl-2.0.html)
open source license.