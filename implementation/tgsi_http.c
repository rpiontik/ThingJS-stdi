//
// Created by rpiontik on 16.05.20.
//

#include "tgsi_http.h"

#include "freertos/FreeRTOSConfig.h"
#include "freertos/FreeRTOS.h"
#include <mongoose.h>
#include <esp_log.h>
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"
#include "mbedtls/base64.h"

#include "sdti_utils.h"
#include "thingjs_board.h"
#include "thingjs_core.h"

#define  INTERFACE_NAME "http"
#define  SYS_PROP_JOBS  "$requests"

#define HTTP_DEFAULT_TIMEOUT 1000

const char TAG_HTTP[] = INTERFACE_NAME;
const char HTTP_METHOD[] = "method";
const char HTTP_URL[] = "url";
const char HTTP_HEADERS[] = "headers";
const char HTTP_PARAMS[] = "params";
const char HTTP_TIMEOUT[] = "timeout";
const char HTTP_CFG_CONTENT_TYPE[] = "content_type";
const char HTTP_CFG_TRANSFER_ENCODING[] = "transfer_encoding";
const char HTTP_DATA[] = "data";
const char HTTP_AUTH[] = "auth";
const char HTTP_AUTH_USERNAME[] = "username";
const char HTTP_AUTH_PASSWORD[] = "password";

const char CLRF[] = "\r\n";

const char HTTP_CONTENT_TYPE[] = "Content-Type";
const char HTTP_CONTENT_LENGTH[] = "Content-Length";
const char HTTP_TRANSFER_ENCODING[] = "Transfer-Encoding";
const char HTTP_CONTENT_TYPE_FORM_URLENCODED[] = "application/x-www-form-urlencoded";
const char HTTP_CONTENT_TYPE_MULTIPART[] = "multipart/form-data";
const char HTTP_CONTENT_TYPE_TEXT[] = "text/plain";
const char HTTP_CONTENT_TYPE_JSON[] = "application/json";

#define APPNAME pcTaskGetTaskName(NULL)

#define SWRITEP_(buffer, length) if(0 > write(context->connect, buffer, length)) return MJS_INTERNAL_ERROR;
#define SWRITEP(buffer, length) {fprintf(stdout, "%.*s", length, (char*)buffer); if(0 > write(context->connect, buffer, length)) return MJS_INTERNAL_ERROR;}
#define SWRITES_(buffer, length) if(0 > write(context.connect, buffer, length)) goto on_socket_error;
#define SWRITES(buffer, length) {fprintf(stdout, "%.*s", length, (char*)buffer); if(0 > write(context.connect, buffer, length)) goto on_socket_error;}

#define SWRITE__(buffer, length) fprintf(stdout, "%.*s", length, (char*)buffer);
#define SWRITE_(buffer, length) {};

typedef enum {
    http_get = 0,
    http_head = 1,
    http_post = 2,
    http_put = 3,
    http_delete = 4,
    http_connect = 5,
    http_options = 6,
    http_trace = 7,
    http_patch = 8,
    http_custom = 9
} http_method_type;

typedef enum {
    http_ct_www_form_encoded = 0,
    http_ct_multipart = 1,
    http_ct_text = 2,
    http_ct_json = 3,
    http_ct_custom = 4
} http_content_type;

typedef enum {
    http_te_none = 0,
    http_te_chunked = 1,
    // http_te_compress, // not supported
    // http_te_deflate,  // not supported
    // http_te_gzip,     // not supported
    // http_te_identity  // not supported
    http_te_custom = 2
} http_transfer_encoding;

#define HTTP_METHODS_NUMBER     9

const struct {
    http_method_type method;
    char name[8];
} methods[HTTP_METHODS_NUMBER] = {
        {.method = http_get,        .name = "GET"},
        {.method = http_head,       .name = "HEAD"},
        {.method = http_post,       .name = "POST"},
        {.method = http_put,        .name = "PUT"},
        {.method = http_delete,     .name = "DELETE"},
        {.method = http_connect,    .name = "CONNECT"},
        {.method = http_options,    .name = "OPTIONS"},
        {.method = http_trace,      .name = "TRACE"},
        {.method = http_patch,      .name = "PATCH"}
};

#define HTTP_BUFFER_LENGTH   256
char http_buffer[HTTP_BUFFER_LENGTH] = {0};
xSemaphoreHandle http_buffer_mutex;

struct st_http_context {
    const char * uri;
    struct mg_str scheme;
    struct mg_str user_info;
    struct mg_str host;
    struct mg_str path;
    struct mg_str query;
    struct mg_str fragment;
    unsigned int port;
    int timeout;
    bool use_ssl;
    http_method_type method;
    mjs_val_t custom_method;
    http_content_type content_type;
    mjs_val_t custom_content_type;
    http_transfer_encoding transfer_encoding;
    mjs_val_t custom_transfer_encoding;
    int connect;
    size_t content_length;
};

static int thingjsHTTPConnect(struct mg_str host, unsigned int port) {
    const struct addrinfo hints = {
            .ai_family = AF_INET,
            .ai_socktype = SOCK_STREAM,
    };

    struct addrinfo *res;
    int soc;

    char *c_host = malloc(host.len + 1);
    snprintf(c_host, host.len + 1, "%.*s", (int) host.len, host.p);

    char c_port[16];
    snprintf(c_port, 16, "%d", port);

    int err = getaddrinfo(c_host, c_port, &hints, &res);

    free(c_host);

    if (err != 0 || res == NULL)
        return 0;

    soc = socket(res->ai_family, res->ai_socktype, 0);
    if (soc < 0)
        return 0;

    if (connect(soc, res->ai_addr, res->ai_addrlen) != 0) {
        close(soc);
        freeaddrinfo(res);
        return 0;
    }
    freeaddrinfo(res);

    return soc;
}

static mjs_err_t thingjsHTTPParseURI(struct mjs *mjs, mjs_val_t config, struct st_http_context * context) {
    //Parse URL
    mjs_val_t cfg_url = mjs_get(mjs, config, HTTP_URL, ~0);
    if (mjs_is_string(cfg_url)) {
        context->uri = mjs_get_cstring(mjs, &cfg_url);

        ESP_LOGD(TAG_HTTP, "Request to [%s]", context->uri);

        if (mg_parse_uri(mg_mk_str(context->uri), &context->scheme, &context->user_info, &context->host, &context->port,
                         &context->path, &context->query, &context->fragment) != 0) {
            mjs_set_errorf(mjs, MJS_INTERNAL_ERROR, "%s/%s: Cannot parse url [%s]",
                    APPNAME, TAG_HTTP, context->uri);
            return MJS_INTERNAL_ERROR;
        }

        // Check https scheme
        context->use_ssl = mg_vcmp(&context->scheme, "https") == 0;
        context->port = context->port == 0 ? (context->use_ssl ? 443 : 80) : context->port;
    } else {
        mjs_set_errorf(mjs,MJS_INTERNAL_ERROR, "%s/%s: Request's config field 'url' is required",
                APPNAME, TAG_HTTP);
        return MJS_INTERNAL_ERROR;
    }
    /*
    ESP_LOGD(TAG_HTTP,
             "URL parsed: scheme:[%.*s], user_info:[%.*s], host:[%.*s], port:[%d], path:[%.*s], query:[%.*s], fragment:[%.*s]",
             (int) context->scheme.len, context->scheme.p,
             (int) context->user_info.len, context->user_info.p,
             (int) context->host.len, context->host.p,
             context->port,
             (int) context->path.len, context->path.p,
             (int) context->query.len, context->query.p,
             (int) context->fragment.len, context->fragment.p
    );
     */

    return MJS_OK;
}

static mjs_err_t thingjsHTTPParseMethod(struct mjs *mjs, mjs_val_t config, struct st_http_context * context) {
    context->method = http_get;
    mjs_val_t cfg_method = mjs_get(mjs, config, HTTP_METHOD, ~0);
    if (mjs_is_number(cfg_method)) {
        http_method_type method = (http_method_type) mjs_get_int(mjs, cfg_method);
        if(method >= http_custom) {
            mjs_set_errorf(mjs, MJS_INTERNAL_ERROR, "%s/%s: Unknown http method [%s]", APPNAME, TAG_HTTP,
                           mjs_get_cstring(mjs, &cfg_method));
            return MJS_INTERNAL_ERROR;
        } else {
            context->method = method;
        }
    } else if(mjs_is_string(cfg_method)) {
        context->custom_method = cfg_method;
        context->method = http_custom;
    }
    return MJS_OK;
}

static mjs_err_t thingjsHTTPParseContentType(struct mjs *mjs, mjs_val_t config, struct st_http_context * context) {
    context->content_type = http_ct_www_form_encoded;
    mjs_val_t cfg_content_type = mjs_get(mjs, config, HTTP_CFG_CONTENT_TYPE, ~0);
    if (!mjs_is_undefined(cfg_content_type)) {
        http_content_type content_type;
        if(mjs_is_number(cfg_content_type) && ((content_type = mjs_get_int(mjs, cfg_content_type)) < http_ct_custom)) {
            context->content_type = content_type;
        } else if (mjs_is_string(cfg_content_type)) {
            context->content_type = http_ct_custom;
            context->custom_content_type = cfg_content_type;
        } else {
            mjs_set_errorf(mjs, MJS_INTERNAL_ERROR, "%s/%s: Unknown Content-Type", APPNAME, TAG_HTTP);
            return MJS_INTERNAL_ERROR;
        }
    }

    return MJS_OK;
}

static mjs_err_t thingjsHTTPParseTransferEncoding(struct mjs *mjs, mjs_val_t config, struct st_http_context * context) {
    context->transfer_encoding = http_te_none;
    mjs_val_t cfg_transfer_encoding = mjs_get(mjs, config, HTTP_CFG_TRANSFER_ENCODING, ~0);
    if (!mjs_is_undefined(cfg_transfer_encoding)) {
        http_transfer_encoding transfer_encoding;
        if(mjs_is_number(cfg_transfer_encoding) && ((transfer_encoding = mjs_get_int(mjs, cfg_transfer_encoding)) <= http_te_chunked)) {
            context->transfer_encoding = transfer_encoding;
        } else if(mjs_is_string(cfg_transfer_encoding)) {
            context->custom_transfer_encoding = cfg_transfer_encoding;
            context->transfer_encoding = http_te_custom;
        } else {
            mjs_set_errorf(mjs, MJS_INTERNAL_ERROR, "%s/%s: Unknown Transfer-Encoding", APPNAME, TAG_HTTP);
            return MJS_INTERNAL_ERROR;
        }
    }
    return MJS_OK;
}

static mjs_err_t thingjsHTTPParseTimeout(struct mjs *mjs, mjs_val_t config, struct st_http_context * context) {
    mjs_val_t cfg_timeout = mjs_get(mjs, config, HTTP_TIMEOUT, ~0);
    context->timeout = mjs_is_number(cfg_timeout) ? mjs_get_int(mjs, cfg_timeout) : HTTP_DEFAULT_TIMEOUT;
    return MJS_OK;
}

static mjs_err_t thingjsHTTPAppendTop(struct mjs *mjs, mjs_val_t config, struct st_http_context * context) {
    if(xSemaphoreTake(http_buffer_mutex, portMAX_DELAY ) == pdTRUE) {
        int len = snprintf(http_buffer, HTTP_BUFFER_LENGTH,
                           "%s %.*s",
                           context->method == http_custom
                                ? mjs_get_cstring(mjs, &context->custom_method)
                                : methods[context->method].name,
                           (int) context->path.len, context->path.p
        );
        SWRITEP(http_buffer, len);

        if(context->query.len > 0) {
            SWRITEP("?", 1);
            SWRITEP(context->query.p, context->query.len);
        }

        // PARAMS (PATH)
        mjs_val_t cfg_params = mjs_get(mjs, config, HTTP_PARAMS, ~0);
        if (mjs_is_object(cfg_params)) {
            SWRITEP(context->query.len == 0? "?" : "&", 1);

            mjs_val_t cfg_param_iterator = MJS_UNDEFINED;
            mjs_val_t cfg_param_name = MJS_UNDEFINED;
            bool is_first = true;
            while ((cfg_param_name = mjs_next(mjs, cfg_params, &cfg_param_iterator)) != MJS_UNDEFINED) {
                mjs_val_t cfg_param_value = mjs_get_v(mjs, cfg_params, cfg_param_name);
                const char * c_param_name = mjs_get_cstring(mjs, &cfg_param_name);

                if(!is_first)
                SWRITEP("&", 1);
                SWRITEP(c_param_name, strlen(c_param_name));
                SWRITEP("=", 1);

                if (mjs_is_string(cfg_param_value)) {
                    const char * c_param_value =  mjs_get_cstring(mjs, &cfg_param_value);
                    SWRITEP(c_param_value, strlen(c_param_value));
                } else {
                    mjs_sprintf(cfg_param_value, mjs, http_buffer, HTTP_BUFFER_LENGTH);
                    SWRITEP(http_buffer, strlen(http_buffer));
                }

                is_first = false;
            }
        }

        if(context->fragment.len > 0) {
            SWRITEP("#", 1);
            SWRITEP(context->fragment.p, context->fragment.len);
        }

        len = snprintf(http_buffer, HTTP_BUFFER_LENGTH,
                       " HTTP/1.0\r\n"
                       "Host: %.*s\r\n"
                       "User-Agent: ThingJS\r\n",
                       (int) context->host.len, context->host.p
        );

        SWRITEP(http_buffer, len);

        xSemaphoreGive(http_buffer_mutex);
        return MJS_OK;
    } else
        return MJS_INTERNAL_ERROR;
}

static mjs_err_t thingjsHTTPAppendHeaders(struct mjs *mjs, mjs_val_t config, struct st_http_context * context) {
    if(xSemaphoreTake(http_buffer_mutex, portMAX_DELAY ) == pdTRUE) {
        mjs_val_t cfg_headers = mjs_get(mjs, config, HTTP_HEADERS, ~0);
        if (mjs_is_object(cfg_headers)) {
            mjs_val_t cfg_header_iterator = MJS_UNDEFINED;
            mjs_val_t cfg_header_name = MJS_UNDEFINED;

            while ((cfg_header_name = mjs_next(mjs, cfg_headers, &cfg_header_iterator)) != MJS_UNDEFINED) {
                mjs_val_t cfg_header_value = mjs_get_v(mjs, cfg_headers, cfg_header_name);
                const char * c_header_name =  mjs_get_cstring(mjs, &cfg_header_name);
                SWRITEP(c_header_name, strlen(c_header_name));
                SWRITEP(": ", 2);

                if (mjs_is_string(cfg_header_value)) {
                    size_t size;
                    const char * c_header_value =  mjs_get_string(mjs, &cfg_header_value, &size);
                    SWRITEP(c_header_value, size);
                } else {
                    mjs_sprintf(cfg_header_value, mjs, http_buffer, HTTP_BUFFER_LENGTH);
                    SWRITEP(http_buffer, strlen(http_buffer));
                }
                SWRITEP(CLRF, 2);
            }
        }
        xSemaphoreGive(http_buffer_mutex);
        return MJS_OK;
    } else
        return MJS_INTERNAL_ERROR;
}

static mjs_err_t thingjsHTTPAppendContentType(struct mjs *mjs, mjs_val_t config, struct st_http_context * context) {
    SWRITEP(HTTP_CONTENT_TYPE, strlen(HTTP_CONTENT_TYPE));
    SWRITEP(": ", 2);
    switch(context->content_type) {
        case http_ct_multipart: {
            SWRITEP(HTTP_CONTENT_TYPE_MULTIPART, strlen(HTTP_CONTENT_TYPE_MULTIPART));
            break;
        }
        case http_ct_www_form_encoded: {
            SWRITEP(HTTP_CONTENT_TYPE_FORM_URLENCODED, strlen(HTTP_CONTENT_TYPE_FORM_URLENCODED));
            break;
        }
        case http_ct_text: {
            SWRITEP(HTTP_CONTENT_TYPE_TEXT, strlen(HTTP_CONTENT_TYPE_TEXT));
            break;
        }
        case http_ct_json: {
            SWRITEP(HTTP_CONTENT_TYPE_JSON, strlen(HTTP_CONTENT_TYPE_JSON));
            break;
        }
        case http_ct_custom: {
            size_t size;
            const char * content_type = mjs_get_string(mjs, &context->custom_content_type, &size);
            SWRITEP(content_type, size);
        }
    }
    SWRITEP(CLRF, 2);
    return MJS_OK;
}

static mjs_err_t thingjsHTTPAppendAuth(struct mjs *mjs, mjs_val_t config, struct st_http_context * context) {
    if(xSemaphoreTake(http_buffer_mutex, portMAX_DELAY ) == pdTRUE) {
        mjs_val_t cfg_auth = mjs_get(mjs, config, HTTP_AUTH, ~0);
        const char *username = "", *password = "";
        if (mjs_is_object(cfg_auth)) {
            mjs_val_t auth_username = mjs_get(mjs, cfg_auth, HTTP_AUTH_USERNAME, ~0);
            if (mjs_is_string(auth_username))
                username = mjs_get_cstring(mjs, &auth_username);

            mjs_val_t auth_password = mjs_get(mjs, cfg_auth, HTTP_AUTH_PASSWORD, ~0);
            if (mjs_is_string(auth_password))
                password = mjs_get_cstring(mjs, &auth_password);

            SWRITEP("Authorization: Basic ", 21);

            int auth_len = snprintf(http_buffer, HTTP_BUFFER_LENGTH, "%s:%s", username, password);
            unsigned char * auth_base64 = (unsigned char *)&http_buffer[auth_len + 1];

            size_t secret_len = 0;
            mbedtls_base64_encode(auth_base64, HTTP_BUFFER_LENGTH - auth_len - 1,
                    &secret_len, (unsigned char *)http_buffer, auth_len);

            SWRITEP(auth_base64, secret_len);
            SWRITEP(CLRF, 2);
        }

        xSemaphoreGive(http_buffer_mutex);
        return MJS_OK;
    } else
        return MJS_INTERNAL_ERROR;
}

static mjs_err_t thingjsHTTPAppendTransferEncoding(struct st_http_context * context) {
    if(context->transfer_encoding == http_te_chunked) {
        char header[32];
        int header_len = snprintf(header, 32, "%s: %s\r\n", HTTP_TRANSFER_ENCODING, "chunked");
        SWRITEP(header, header_len);
    }
    return MJS_OK;
}

static mjs_err_t thingjsHTTPAppendContentLength(struct st_http_context * context) {
    char header[32];
    int header_len = snprintf(header, 32, "%s: %d\r\n", HTTP_CONTENT_LENGTH, context->content_length);
    SWRITEP(header, header_len);
    return MJS_OK;
}

static mjs_err_t thingjsHTTPAppendChunk(struct st_http_context * context, const char * chunk, size_t size) {
    char header[16];
    int header_len = snprintf(header, 32, "%x\r\n", size);
    SWRITEP(header, header_len);
    SWRITEP(chunk, size);
    SWRITEP(CLRF, 2);
    return MJS_OK;
}

static void thingjsHTTPAppendURLEncodeStr(struct mbuf * mb, const char * str, size_t size) {
    const struct mg_str safe = mg_mk_str("._-$,;~()/");
    char hex_buf[3];
    for(size_t i = 0; i < size; i++) {
        const unsigned char c = *((const unsigned char *) str + i);
        if (isalnum(c) || mg_strchr(safe, c) != NULL) {
            mbuf_append(mb, &c, 1);
        } else if (c == ' ') {
            mbuf_append(mb, "+", 1);
        } else {
            snprintf(hex_buf, 3, "%%%02x", c);
            mbuf_append(mb, hex_buf, 3);
        }
    }
}

static mjs_err_t thingjsHTTPAppendPOSTBody(struct mjs *mjs, mjs_val_t config, struct st_http_context * context) {
    mjs_err_t res = MJS_OK;
    mjs_val_t cfg_data = mjs_get(mjs, config, HTTP_DATA, ~0);
    if(context->transfer_encoding == http_te_chunked) {
        SWRITEP(CLRF, 2);
    }

    // Custom body generator
    if(mjs_is_function(cfg_data)) {
        mjs_val_t part = MJS_UNDEFINED;
        do {
            res = mjs_apply(mjs, &part, cfg_data, config, 0, NULL);
            if (res != MJS_OK) {
                mjs_return(mjs, res);
                return res;
            }

            size_t chunk_size;
            const char *chunk;
            bool is_sem_take = false;

            if (mjs_is_undefined(part) || mjs_is_null(part)) {
                if(context->transfer_encoding == http_te_chunked)
                    res = thingjsHTTPAppendChunk(context, "", 0);
                    SWRITEP(CLRF, 2);
                break;
            } else if (mjs_is_string(part)) {
                chunk = mjs_get_string(mjs, &part, &chunk_size);
            } else {
                if(xSemaphoreTake(http_buffer_mutex, portMAX_DELAY ) == pdTRUE) {
                    is_sem_take = true;
                    mjs_sprintf(part, mjs, http_buffer, HTTP_BUFFER_LENGTH);
                    chunk = http_buffer;
                    chunk_size = strlen(http_buffer);
                } else
                    return MJS_INTERNAL_ERROR;
            }

            context->content_length += chunk_size;

            switch (context->transfer_encoding) {
                case http_te_custom:
                case http_te_none: {
                    res = thingjsHTTPAppendContentLength(context);
                    SWRITEP(CLRF, 2);
                    SWRITEP(chunk, chunk_size);
                    break;
                }
                case http_te_chunked: {
                    res = thingjsHTTPAppendChunk(context, chunk, chunk_size);
                    break;
                }
            }

            if(is_sem_take)
                xSemaphoreGive(http_buffer_mutex);

        } while((res == MJS_OK) && (context->transfer_encoding == http_te_chunked));
    } else if(mjs_is_object(cfg_data)) {
        switch(context->content_type) {
            case http_ct_www_form_encoded: { //application/x-www-form-urlencoded
                struct mbuf mb;
                mbuf_init(&mb, 0);

                mjs_val_t cfg_field_iterator = MJS_UNDEFINED;
                mjs_val_t cfg_field_name = MJS_UNDEFINED;
                bool is_first = true;

                while ((cfg_field_name = mjs_next(mjs, cfg_data, &cfg_field_iterator)) != MJS_UNDEFINED) {
                    if(!is_first)
                        mbuf_append(&mb, "&", 1);

                    const char * c_field_name =  mjs_get_cstring(mjs, &cfg_field_name);
                    thingjsHTTPAppendURLEncodeStr(&mb, c_field_name, strlen(c_field_name));

                    mbuf_append(&mb, "=", 1);

                    mjs_val_t cfg_field_value = mjs_get_v(mjs, cfg_data, cfg_field_name);
                    if (mjs_is_string(cfg_field_value)) {
                        size_t size;
                        const char * c_header_value =  mjs_get_string(mjs, &cfg_field_value, &size);
                        thingjsHTTPAppendURLEncodeStr(&mb, c_header_value, size);
                    } else {
                        if(xSemaphoreTake(http_buffer_mutex, portMAX_DELAY ) == pdTRUE) {
                            mjs_sprintf(cfg_field_value, mjs, http_buffer, HTTP_BUFFER_LENGTH);
                            thingjsHTTPAppendURLEncodeStr(&mb, http_buffer, strlen(http_buffer));
                            xSemaphoreGive(http_buffer_mutex);
                        } else
                            return MJS_INTERNAL_ERROR;
                    }

                    if(context->transfer_encoding == http_te_chunked) {
                        thingjsHTTPAppendChunk(context, mb.buf, mb.len);
                        mbuf_clear(&mb);
                    }

                    is_first = false;
                }

                if(context->transfer_encoding == http_te_chunked) {
                    thingjsHTTPAppendChunk(context, "", 0);
                } else {
                    context->content_length = mb.len;
                    res = thingjsHTTPAppendContentLength(context);
                    SWRITEP(CLRF, 2);
                    SWRITEP(mb.buf, mb.len);
                }

                mbuf_free(&mb);
                break;
            }
            case http_ct_text:              //raw
                break;
            case http_ct_json: {
                char *json = NULL;
                if (MJS_OK == (res = mjs_json_stringify(mjs, cfg_data, NULL, 0, &json))) {
                    context->content_length = strlen(json);
                    if(context->transfer_encoding == http_te_chunked) {
                        thingjsHTTPAppendChunk(context, json, context->content_length);
                        thingjsHTTPAppendChunk(context, "", 0);
                    } else {
                        res = thingjsHTTPAppendContentLength(context);
                        SWRITEP(CLRF, 2);
                        SWRITEP(json, context->content_length);
                    }
                    free(json);
                }
                break;
            }
            case http_ct_multipart:     //multipart/form-data
                break;
            case http_ct_custom:
                break;
        }
    }

    return res;
}

static mjs_err_t thingjsHTTPReadResponse(struct mjs *mjs, mjs_val_t config, struct st_http_context * context) {
    if(xSemaphoreTake(http_buffer_mutex, portMAX_DELAY ) == pdTRUE) {
        int read_count = 0;
        do {
            bzero(http_buffer, HTTP_BUFFER_LENGTH);
            read_count = read(context->connect, http_buffer, HTTP_BUFFER_LENGTH - 1);
            for (int i = 0; i < read_count; i++) {
                putchar(http_buffer[i]);
            }
        } while (read_count > 0);
        xSemaphoreGive(http_buffer_mutex);
        return MJS_OK;
    } else
        return MJS_INTERNAL_ERROR;
}

static mjs_err_t thingjsHTTPApplyTimeout( struct st_http_context * context) {
    struct timeval receiving_timeout;
    receiving_timeout.tv_sec = 0;
    receiving_timeout.tv_usec = context->timeout;
    if (setsockopt(context->connect, SOL_SOCKET, SO_RCVTIMEO, &receiving_timeout,
                   sizeof(receiving_timeout)) < 0)
        return MJS_INTERNAL_ERROR;
    else
        return  MJS_OK;
}

static void thingjsHTTPRequest(struct mjs *mjs) {
    //Get function params
    mjs_err_t result = MJS_OK;
    mjs_val_t config = mjs_arg(mjs, 0);   //Request config
    mjs_val_t callback = mjs_arg(mjs, 1);   //Callback function

    //Get context
    mjs_val_t this = mjs_get_this(mjs); //this interface object
    mjs_val_t jobs = mjs_get(mjs, this, SYS_PROP_JOBS, ~0); //Active sockets

    const char *app_name = pcTaskGetTaskName(NULL);

    //Parse request params
    if (mjs_is_array(jobs) && mjs_is_object(this) && mjs_is_function(callback)
        && mjs_is_object(config)) {

        struct st_http_context context = {0};
        if(
                (MJS_OK == (result = thingjsHTTPParseURI(mjs, config, &context)))
                && (MJS_OK == (result = thingjsHTTPParseMethod(mjs, config, &context)))
                && (MJS_OK == (result = thingjsHTTPParseTimeout(mjs, config, &context)))
                && (MJS_OK == (result = thingjsHTTPParseContentType(mjs, config, &context)))
                && (MJS_OK == (result = thingjsHTTPParseTransferEncoding(mjs, config, &context)))
        ) {

            //Open socket connection
            context.connect = thingjsHTTPConnect(context.host, context.port);

            if(
                    !context.connect
                    || (MJS_OK != thingjsHTTPAppendTop(mjs, config, &context))
                    || (MJS_OK != thingjsHTTPAppendHeaders(mjs, config, &context))
                    || (MJS_OK != thingjsHTTPAppendContentType(mjs, config, &context))
                    || (MJS_OK != thingjsHTTPAppendTransferEncoding(&context))
                    || (MJS_OK != thingjsHTTPAppendAuth(mjs, config, &context))
            ) goto on_socket_error;

            if((context.method == http_post) || (context.method == http_put) || (context.method == http_patch)) {
                switch (result = thingjsHTTPAppendPOSTBody(mjs, config, &context)) {
                    case MJS_OK: break;
                    case MJS_INTERNAL_ERROR : goto on_socket_error;
                    default: {
                        if(context.connect)
                            close(context.connect);
                        mjs_return(mjs, result);
                        return;
                    };
                }
            }

            if(MJS_OK != thingjsHTTPApplyTimeout(&context))
                goto on_socket_error;

            thingjsHTTPReadResponse(mjs, config, &context);

            if(context.connect)
                close(context.connect);
        }

        mjs_return(mjs, result);
        return;
    on_socket_error:
        //todo NEED ERROR CALLBACK
        ESP_LOGD(TAG_HTTP, "Cannot open connection with [%s]", context.uri);
        if(context.connect)
            close(context.connect);
        mjs_return(mjs, MJS_OK);
    } else {
        mjs_set_errorf(mjs, MJS_INTERNAL_ERROR, "%s/%s: Incorrect params of function request", app_name, TAG_HTTP);
        mjs_return(mjs, MJS_INTERNAL_ERROR);
    }
}

mjs_val_t thingjsHTTPConstructor(struct mjs *mjs, cJSON *params) {
    //Create mjs object
    mjs_val_t interface = mjs_mk_object(mjs);

    //Create requests collection
    stdi_setProtectedProperty(mjs, interface, SYS_PROP_JOBS, mjs_mk_array(mjs));

    //Bind functions
    stdi_setProtectedProperty(mjs, interface, "request",
                              mjs_mk_foreign_func(mjs, (mjs_func_ptr_t) thingjsHTTPRequest));

    //Consts
    // Methods
    for(int i = 0; i < HTTP_METHODS_NUMBER; i++) {
        char buffer[32];
        snprintf(buffer, 32, "M_%s", methods[i].name);
        stdi_setProtectedProperty(mjs, interface, buffer, mjs_mk_number(mjs, (int)methods[i].method));
    }

    // Transfer type
    stdi_setProtectedProperty(mjs, interface, "TE_NONE", mjs_mk_number(mjs, (int)http_te_none));
    stdi_setProtectedProperty(mjs, interface, "TE_CHUNKED", mjs_mk_number(mjs, (int)http_te_chunked));

    // Content types
    stdi_setProtectedProperty(mjs, interface, "CT_FORM_URLENCODED", mjs_mk_number(mjs, (int)http_ct_www_form_encoded));
    stdi_setProtectedProperty(mjs, interface, "CT_MULTIPART_FORM_DATA", mjs_mk_number(mjs, (int)http_ct_multipart));
    stdi_setProtectedProperty(mjs, interface, "CT_TEXT", mjs_mk_number(mjs, (int)http_ct_text));
    stdi_setProtectedProperty(mjs, interface, "CT_JSON", mjs_mk_number(mjs, (int)http_ct_json));


    //Return mJS interface object
    return interface;
}

void thingjsHTTPDestructor(struct mjs *mjs, mjs_val_t subject) {
}

void thingjsHTTPRegister(void) {
    static int thingjs_http_cases[] = DEF_CASES(DEF_CASE(RES_VIRTUAL));

    static const struct st_thingjs_interface_manifest interface = {
            .type           = INTERFACE_NAME,
            .constructor    = thingjsHTTPConstructor,
            .destructor     = thingjsHTTPDestructor,
            .cases          = thingjs_http_cases
    };

    http_buffer_mutex = xSemaphoreCreateMutex();

    thingjsRegisterInterface(&interface);
}
