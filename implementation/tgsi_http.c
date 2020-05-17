//
// Created by rpiontik on 16.05.20.
//

#include "tgsi_http.h"

#include "freertos/FreeRTOSConfig.h"
#include "freertos/FreeRTOS.h"
#include <mongoose.h>
#include <freertos/projdefs.h>
#include <freertos/queue.h>
#include <esp_log.h>

#include "sdti_utils.h"
#include "thingjs_board.h"
#include "thingjs_core.h"

#define  INTERFACE_NAME "http"
#define  SYS_PROP_JOBS  "$requests"

#define HTTP_MAX_METHOD_LENGTH  32
#define HTTP_MAX_HEADER_LENGTH  512
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

const char HTTP_CONTENT_TYPE[] = "Content-Type";
const char HTTP_CONTENT_TYPE_APPLICATION[] = "application/x-www-form-urlencoded";
const char HTTP_CONTENT_TYPE_MULTIPART[] = "multipart/form-data";
const char HTTP_CONTENT_TYPE_TEXT[] = "text/plain";

typedef enum {
    http_get,
    http_head,
    http_post,
    http_put,
    http_delete,
    http_connect,
    http_options,
    http_trace,
    http_patch,
    http_unknown
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

static void thingjsHTTPRequest(struct mjs *mjs) {
    //Get function params
    mjs_val_t config = mjs_arg(mjs, 0);   //Request config
    mjs_val_t callback = mjs_arg(mjs, 1);   //Callback function

    //Get context
    mjs_val_t this = mjs_get_this(mjs); //this interface object
    mjs_val_t jobs = mjs_get(mjs, this, SYS_PROP_JOBS, ~0); //Active sockets

    const char *app_name = pcTaskGetTaskName(NULL);

    //Parse request params
    if (mjs_is_array(jobs) && mjs_is_object(this) && mjs_is_function(callback)
        && mjs_is_object(config)) {

        //char http_header[HTTP_MAX_HEADER_LENGTH] = "Connection: keep-alive\r\nUser-Agent: ThingJS\r\n";

        struct mg_str scheme, user_info, host, path, query, fragment;
        unsigned int port = 0;
        const char *uri = NULL;
        bool use_ssl = false;

        //Parse URL
        mjs_val_t cfg_url = mjs_get(mjs, config, HTTP_URL, ~0);
        if (mjs_is_string(cfg_url)) {
            uri = mjs_get_cstring(mjs, &cfg_url);

            ESP_LOGD(TAG_HTTP, "Request to [%s]", uri);

            if (mg_parse_uri(mg_mk_str(uri), &scheme, &user_info, &host, &port,
                             &path, &query, &fragment) != 0) {
                mjs_set_errorf(mjs, MJS_INTERNAL_ERROR, "%s/%s: Cannot parse url [%s]", app_name, TAG_HTTP, uri);
                mjs_return(mjs, MJS_INTERNAL_ERROR);
                return;
            }

            // Check https scheme
            use_ssl = mg_vcmp(&scheme, "https") == 0;
            port = port == 0 ? (use_ssl ? 443 : 80) : port;

        } else {
            mjs_set_errorf(mjs, MJS_INTERNAL_ERROR, "%s/%s: Request's config field 'url' is required", app_name,
                           TAG_HTTP);
            mjs_return(mjs, MJS_INTERNAL_ERROR);
            return;
        }

        ESP_LOGD(TAG_HTTP, "URL parsed: scheme:[%.*s], user_info:[%.*s], host:[%.*s], port:[%d], path:[%.*s], query:[%.*s], fragment:[%.*s]",
                 (int) scheme.len, scheme.p,
                 (int) user_info.len, user_info.p,
                 (int) host.len, host.p,
                 port,
                 (int) path.len, path.p,
                 (int) query.len, query.p,
                 (int) fragment.len, fragment.p
                );


        // METHOD
        http_methods_type method = http_unknown;
        mjs_val_t cfg_method = mjs_get(mjs, config, HTTP_METHOD, ~0);
        if (mjs_is_string(cfg_method)) {
            for (int i = 0; i < HTTP_METHODS_NUMBER; ++i) {
                if (mjs_strcmp(mjs, &cfg_method, methods[i].name, ~0) == 0) {
                    method = methods[i].method;
                    break;
                }
            }

            if (method == http_unknown) {
                mjs_set_errorf(mjs, MJS_INTERNAL_ERROR, "%s/%s: Unknown http method [%s]", app_name, TAG_HTTP,
                               mjs_get_cstring(mjs, &cfg_method));
                mjs_return(mjs, MJS_INTERNAL_ERROR);
                return;
            }

            ESP_LOGD(TAG_HTTP, "Request method [%s]", mjs_get_cstring(mjs, &cfg_method));

        } else {
            method = http_get;
            ESP_LOGD(TAG_HTTP, "Request method [GET]");
        }

        //Timeout
        mjs_val_t cfg_timeout = mjs_get(mjs, config, HTTP_TIMEOUT, ~0);
        int timeout = mjs_is_number(cfg_timeout) ? mjs_get_int(mjs, cfg_timeout) : HTTP_DEFAULT_TIMEOUT;

        ESP_LOGD(TAG_HTTP, "Request timeout [%d]", timeout);

        // HEADERS
        http_content_type content_type = http_ct_text;
        mjs_val_t cfg_headers = mjs_get(mjs, config, HTTP_HEADERS, ~0);
        if (mjs_is_object(cfg_headers)) {
            mjs_val_t cfg_header_iterator = MJS_UNDEFINED;
            mjs_val_t cfg_header_name = MJS_UNDEFINED;

            while ((cfg_header_name = mjs_next(mjs, cfg_headers, &cfg_header_iterator)) != MJS_UNDEFINED) {
                mjs_val_t cfg_header_value = mjs_get_v(mjs, cfg_headers, cfg_header_name);
                if (mjs_strcmp(mjs, &cfg_header_name, HTTP_CONTENT_TYPE, ~0) == 0) {
                    if (mjs_strcmp(mjs, &cfg_header_value, HTTP_CONTENT_TYPE_APPLICATION, ~0) == 0)
                        content_type = http_ct_application;
                    else if (mjs_strcmp(mjs, &cfg_header_value, HTTP_CONTENT_TYPE_MULTIPART, ~0) == 0)
                        content_type = http_ct_multipart;
                    else if (mjs_strcmp(mjs, &cfg_header_value, HTTP_CONTENT_TYPE_TEXT, ~0) == 0)
                        content_type = http_ct_text;
                    ESP_LOGD(TAG_HTTP, "Content-Type header: %s", mjs_get_cstring(mjs, &cfg_header_value));
                } else {
                    ESP_LOGD(TAG_HTTP, "Header: %s : %s", mjs_get_cstring(mjs, &cfg_header_name), mjs_get_cstring(mjs, &cfg_header_value));
                }
            }
        }

        // PARAMS (PATH)
        mjs_val_t cfg_params = mjs_get(mjs, config, HTTP_PARAMS, ~0);
        if (mjs_is_object(cfg_params)) {
            mjs_val_t cfg_param_iterator = MJS_UNDEFINED;
            mjs_val_t cfg_param_name = MJS_UNDEFINED;

            while ((cfg_param_name = mjs_next(mjs, cfg_params, &cfg_param_iterator)) != MJS_UNDEFINED) {
                mjs_val_t cfg_param_value = mjs_get_v(mjs, cfg_params, cfg_param_name);

                char buffer[128];
                mjs_sprintf(cfg_param_value, mjs, buffer, 128);

                // void mjs_fprintf(mjs_val_t v, struct mjs *mjs, FILE *fp);
                ESP_LOGD(TAG_HTTP, "Params: %s : %s", mjs_get_cstring(mjs, &cfg_param_name), buffer);
            }
        }

        //Auth
        mjs_val_t cfg_auth = mjs_get(mjs, config, HTTP_AUTH, ~0);
        bool use_auth = false;
        const char * username = "", * password = "";
        if (mjs_is_object(cfg_auth)) {
            use_auth = true;
            mjs_val_t auth_username = mjs_get(mjs, cfg_auth, HTTP_AUTH_USERNAME, ~0);
            if (mjs_is_string(auth_username))
                username = mjs_get_cstring(mjs, &auth_username);

            mjs_val_t auth_password = mjs_get(mjs, cfg_auth, HTTP_AUTH_PASSWORD, ~0);
            if (mjs_is_string(auth_password))
                password = mjs_get_cstring(mjs, &auth_password);

            ESP_LOGD(TAG_HTTP, "Auth: username:[%s], password:[%s]", username, password);
        }
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

    thingjsRegisterInterface(&interface);
}
