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
const char HTTP_DATA[] = "data";
const char HTTP_AUTH[] = "auth";
const char HTTP_AUTH_USERNAME[] = "username";
const char HTTP_AUTH_PASSWORD[] = "password";

const char CLRF[] = "\r\n";

const char HTTP_CONTENT_TYPE[] = "Content-Type";
const char HTTP_CONTENT_TYPE_APPLICATION[] = "application/x-www-form-urlencoded";
const char HTTP_CONTENT_TYPE_MULTIPART[] = "multipart/form-data";
const char HTTP_CONTENT_TYPE_TEXT[] = "text/plain";

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
    http_unknown = 9
} http_methods_type;

typedef enum {
    http_ct_application,
    http_ct_multipart,
    http_ct_text,
} http_content_type;

#define HTTP_METHODS_NUMBER     9

struct {
    http_methods_type method;
    char name[8];
} methods[HTTP_METHODS_NUMBER] = {
        {.method = http_get, .name = "GET"},
        {.method = http_head, .name = "HEAD"},
        {.method = http_post, .name = "POST"},
        {.method = http_put, .name = "PUT"},
        {.method = http_delete, .name = "DELETE"},
        {.method = http_connect, .name = "CONNECT"},
        {.method = http_options, .name = "OPTIONS"},
        {.method = http_trace, .name = "TRACE"},
        {.method = http_patch, .name = "PATCH"}
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
    http_methods_type method;
    http_content_type content_type;
    int connect;
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
             (int) scheme.len, scheme.p,
             (int) user_info.len, user_info.p,
             (int) host.len, host.p,
             port,
             (int) path.len, path.p,
             (int) query.len, query.p,
             (int) fragment.len, fragment.p
    );
     */

    return MJS_OK;
}

static mjs_err_t thingjsHTTPParseMethod(struct mjs *mjs, mjs_val_t config, struct st_http_context * context) {
    context->method = http_unknown;
    mjs_val_t cfg_method = mjs_get(mjs, config, HTTP_METHOD, ~0);
    if (mjs_is_string(cfg_method)) {
        for (int i = 0; i < HTTP_METHODS_NUMBER; ++i) {
            if (mjs_strcmp(mjs, &cfg_method, methods[i].name, ~0) == 0) {
                context->method = methods[i].method;
                break;
            }
        }

        if (context->method == http_unknown) {
            mjs_set_errorf(mjs, MJS_INTERNAL_ERROR, "%s/%s: Unknown http method [%s]", APPNAME, TAG_HTTP,
                           mjs_get_cstring(mjs, &cfg_method));
            return MJS_INTERNAL_ERROR;
        }
    } else {
        context->method = http_get;
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
                           "%s %.*s://%.*s",
                           methods[context->method].name,
                           (int) context->scheme.len, context->scheme.p,
                           (int) context->host.len, context->host.p
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
        context->content_type = http_ct_text;
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
                    const char * c_header_value =  mjs_get_cstring(mjs, &cfg_header_value);
                    SWRITEP(c_header_value, strlen(c_header_value));

                    if (mjs_strcmp(mjs, &cfg_header_name, HTTP_CONTENT_TYPE, ~0) == 0) {
                        if (mjs_strcmp(mjs, &cfg_header_value, HTTP_CONTENT_TYPE_APPLICATION, ~0) == 0)
                            context->content_type = http_ct_application;
                        else if (mjs_strcmp(mjs, &cfg_header_value, HTTP_CONTENT_TYPE_MULTIPART, ~0) == 0)
                            context->content_type = http_ct_multipart;
                        else if (mjs_strcmp(mjs, &cfg_header_value, HTTP_CONTENT_TYPE_TEXT, ~0) == 0)
                            context->content_type = http_ct_text;
                    }
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

static mjs_err_t thingjsHTTPAppendPOSTBody(struct mjs *mjs, mjs_val_t config, struct st_http_context * context) {
    mjs_val_t cfg_data = mjs_get(mjs, config, HTTP_DATA, ~0);

    // Custom body generator
    if(mjs_is_function(cfg_data)) {
        while(true) {
            mjs_val_t part = MJS_UNDEFINED;
            mjs_err_t res = mjs_apply(mjs, &part, cfg_data, config, 0, NULL);
            if (res != MJS_OK) {
                mjs_return(mjs, res);
                return res;
            }
            if (mjs_is_string(part)) {
                size_t size;
                const char *c_part = mjs_get_string(mjs, &part, &size);
                SWRITEP(c_part, size);
            } else if (mjs_is_undefined(part) || mjs_is_null(part)) {
                break;
            } else {
                if(xSemaphoreTake(http_buffer_mutex, portMAX_DELAY ) == pdTRUE) {
                    mjs_sprintf(part, mjs, http_buffer, HTTP_BUFFER_LENGTH);
                    SWRITEP(http_buffer, strlen(http_buffer));
                    xSemaphoreGive(http_buffer_mutex);
                } else
                    return MJS_INTERNAL_ERROR;
            }
        };
    } else if(!mjs_is_undefined(cfg_data)) {
        switch(context->content_type) {
            case http_ct_application:   //application/x-www-form-urlencoded
                if (mjs_is_object(cfg_data)) {

                } else if (mjs_is_string(cfg_data)) {

                }
                break;
            case http_ct_text:          //raw
                break;
            case http_ct_multipart:     //multipart/form-data
                break;
        }
    }

    return MJS_OK;
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
        ) {
            //Open socket connection
            context.connect = thingjsHTTPConnect(context.host, context.port);

            if(
                    !context.connect
                    || (MJS_OK != thingjsHTTPAppendTop(mjs, config, &context))
                    || (MJS_OK != thingjsHTTPAppendHeaders(mjs, config, &context))
                    || (MJS_OK != thingjsHTTPAppendAuth(mjs, config, &context))
            ) goto on_socket_error;

            SWRITES(CLRF, 2);

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
