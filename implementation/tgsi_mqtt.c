//
// Created by rpiontik on 10.09.20.
//

#include "tgsi_mqtt.h"

#include "freertos/FreeRTOSConfig.h"
#include "freertos/FreeRTOS.h"
#include <esp_log.h>

#include "sdti_utils.h"
#include "thingjs_board.h"
#include "thingjs_core.h"
#include "mqtt_client.h"
#include "esp_tls.h"

#define  INTERFACE_NAME "mqtt"
#define  SYS_PROP_CLIENT  "$client"

const char TAG_MQTT[] = INTERFACE_NAME;

struct thingjs_mqtt_context {
    TaskHandle_t process;
    mjs_val_t this;
    struct mjs *mjs;
};

#define MAX_NUMBER_EVENT_PARAMS 3

struct thingjs_mqtt_event {
    mjs_val_t func;
    mjs_val_t this;
    int params_number;
    mjs_val_t args[MAX_NUMBER_EVENT_PARAMS];
};

mjs_val_t thingjs_mqtt_event_callback(struct mjs *context, void *data) {
    struct thingjs_mqtt_event *callback_data = data;
    mjs_val_t result = mjs_apply(context, NULL, callback_data->func, MJS_UNDEFINED,
                                 callback_data->params_number, callback_data->args);
    free(callback_data);
    return result;
}

static void thingjs_mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    struct thingjs_mqtt_context *context = handler_args;
    esp_mqtt_event_handle_t event = event_data;
    struct thingjs_mqtt_event func_data = {
            .args = {MJS_UNDEFINED},
            .func = MJS_UNDEFINED,
            .this = context->this,
            .params_number = 0
    };
    char *event_name = NULL;
    ESP_LOGD(TAG_MQTT, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            event_name = "onconnected";
            break;
        case MQTT_EVENT_DISCONNECTED:
            event_name = "ondisconnected";
            break;
        case MQTT_EVENT_SUBSCRIBED:
            event_name = "onsubscribed";
            func_data.params_number = 1;
            func_data.args[0] = mjs_mk_number(context->mjs, event->msg_id);
            break;
        case MQTT_EVENT_UNSUBSCRIBED:
            event_name = "onunsubscribed";
            func_data.params_number = 1;
            func_data.args[0] = mjs_mk_number(context->mjs, event->msg_id);
            break;
        case MQTT_EVENT_PUBLISHED:
            event_name = "onpublished";
            func_data.params_number = 1;
            func_data.args[0] = mjs_mk_number(context->mjs, event->msg_id);
            break;
        case MQTT_EVENT_DATA:
            event_name = "ondata";
            func_data.params_number = 2;
            func_data.args[0] = mjs_mk_string(context->mjs, event->topic, event->topic_len, 1);
            func_data.args[1] = mjs_mk_string(context->mjs, event->data, event->data_len, 1);
            break;
        case MQTT_EVENT_ERROR:
            event_name = "onerror";
            func_data.params_number = 2;
            int mbedtls_err = 0;
            esp_err_t err = esp_tls_get_and_clear_last_error(event->error_handle, &mbedtls_err, NULL);
            func_data.args[0] = mjs_mk_number(context->mjs, err);
            func_data.args[1] = mjs_mk_number(context->mjs, mbedtls_err);
            break;
        default:
            ESP_LOGD(TAG_MQTT, "Other event id:%d", event->event_id);
            break;
    }

    if (event_name) {
        func_data.func = mjs_get(context->mjs, context->this, event_name, ~0);
        if (mjs_is_function(func_data.func)) {
            struct thingjs_mqtt_event *callback_data = malloc(sizeof(struct thingjs_mqtt_event));
            memcpy(callback_data, &func_data, sizeof(struct thingjs_mqtt_event));
            if (pdTRUE != thingjsSendCallbackRequest(context->process, thingjs_mqtt_event_callback, callback_data)) {
                free(callback_data);
                ESP_LOGE(TAG_MQTT, "Event stack is full! [%s]", event_name);
            }
        }
    }
}

static void thingjsMQTTPublish(struct mjs *mjs) {
    mjs_val_t result = MJS_OK;
    //Get function params
    mjs_val_t arg0 = mjs_arg(mjs, 0);   //Topic
    mjs_val_t arg1 = mjs_arg(mjs, 1);   //Data
    mjs_val_t arg2 = mjs_arg(mjs, 2);   //QOS
    mjs_val_t arg3 = mjs_arg(mjs, 3);   //Retain
    mjs_val_t this = mjs_get_this(mjs);    //this interface object
    mjs_val_t client = mjs_get(mjs, this, SYS_PROP_CLIENT, ~0); //MQTT Client
    if (mjs_is_string(arg0) && mjs_is_string(arg1) && mjs_is_object(this) && mjs_is_foreign(client)) {
        int qos = mjs_is_number(arg2) ? mjs_get_int(mjs, arg2) : 0;
        int retain = mjs_is_number(arg3) ? mjs_get_int(mjs, arg3) : 0;
        size_t data_len = 0;
        const char * data = mjs_get_string(mjs, &arg1, &data_len);

        result = mjs_mk_number(mjs,
                               esp_mqtt_client_publish(
                                       mjs_get_ptr(mjs, client),
                                       mjs_get_cstring(mjs, &arg0),
                                       data,
                                       data_len,
                                       qos,
                                       retain
                               )
        );
    } else {
        mjs_set_errorf(mjs, MJS_INTERNAL_ERROR, "%s/%s: Incorrect params of function unsubscribe",
                       pcTaskGetTaskName(NULL), TAG_MQTT);
        result = MJS_INTERNAL_ERROR;
    }
    mjs_return(mjs, result);
}


static void thingjsMQTTUnsubscribe(struct mjs *mjs) {
    mjs_val_t result = MJS_OK;
    //Get function params
    mjs_val_t arg0 = mjs_arg(mjs, 0);   //Topic
    mjs_val_t this = mjs_get_this(mjs);    //this interface object
    mjs_val_t client = mjs_get(mjs, this, SYS_PROP_CLIENT, ~0); //MQTT Client
    if (mjs_is_string(arg0) && mjs_is_object(this) && mjs_is_foreign(client)) {
        result = mjs_mk_number(mjs,
                               esp_mqtt_client_unsubscribe(
                                       mjs_get_ptr(mjs, client),
                                       mjs_get_cstring(mjs, &arg0)
                               )
        );
    } else {
        mjs_set_errorf(mjs, MJS_INTERNAL_ERROR, "%s/%s: Incorrect params of function unsubscribe",
                       pcTaskGetTaskName(NULL), TAG_MQTT);
        result = MJS_INTERNAL_ERROR;
    }
    mjs_return(mjs, result);
}

static void thingjsMQTTSubscribe(struct mjs *mjs) {
    mjs_val_t result = MJS_OK;
    //Get function params
    mjs_val_t arg0 = mjs_arg(mjs, 0);   //Topic
    mjs_val_t arg1 = mjs_arg(mjs, 0);   //COS
    mjs_val_t this = mjs_get_this(mjs);    //this interface object
    mjs_val_t client = mjs_get(mjs, this, SYS_PROP_CLIENT, ~0); //MQTT Client
    if (mjs_is_string(arg0) && mjs_is_number(arg1) && mjs_is_object(this) && mjs_is_foreign(client)) {
        result = mjs_mk_number(mjs,
                               esp_mqtt_client_subscribe(
                                       mjs_get_ptr(mjs, client),
                                       mjs_get_cstring(mjs, &arg0),
                                       mjs_get_int(mjs, arg1)
                               )
        );
    } else {
        mjs_set_errorf(mjs, MJS_INTERNAL_ERROR, "%s/%s: Incorrect params of function subscribe",
                       pcTaskGetTaskName(NULL), TAG_MQTT);
        result = MJS_INTERNAL_ERROR;
    }
    mjs_return(mjs, result);
}

static void thingjsMQTTConnect(struct mjs *mjs) {
    mjs_val_t result = MJS_OK;
    //Get function params
    mjs_val_t arg0 = mjs_arg(mjs, 0);   //URI
    mjs_val_t arg1 = mjs_arg(mjs, 1);   //login
    mjs_val_t arg2 = mjs_arg(mjs, 2);   //password
    mjs_val_t this = mjs_get_this(mjs);    //this interface object
    if (mjs_is_string(arg0) && mjs_is_object(this)) {
        const esp_mqtt_client_config_t mqtt_cfg = {
                .uri = mjs_get_cstring(mjs, &arg0),
//                .cert_pem = (const char *) iot_eclipse_org_pem_start,
                .username = mjs_is_string(arg1) ? mjs_get_cstring(mjs, &arg1) : NULL,
                .password = mjs_is_string(arg2) ? mjs_get_cstring(mjs, &arg2) : NULL
        };

        esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
        if (client) {
            struct thingjs_mqtt_context *context = malloc(sizeof(struct thingjs_mqtt_context));
            context->process = xTaskGetCurrentTaskHandle();
            context->mjs = mjs;
            context->this = this;
            esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, thingjs_mqtt_event_handler, context);
            esp_mqtt_client_start(client);
            stdi_setProtectedProperty(mjs, this, SYS_PROP_CLIENT, mjs_mk_foreign(mjs, client));

        } else {
            mjs_set_errorf(mjs, MJS_INTERNAL_ERROR, "%s/%s: Can not create client",
                           pcTaskGetTaskName(NULL), TAG_MQTT);
            result = MJS_INTERNAL_ERROR;
        }
    } else {
        mjs_set_errorf(mjs, MJS_INTERNAL_ERROR, "%s/%s: Incorrect params of function connect",
                       pcTaskGetTaskName(NULL), TAG_MQTT);
        result = MJS_INTERNAL_ERROR;
    }
    mjs_return(mjs, result);
}

static void thingjsMQTTReconnect(struct mjs *mjs) {
    mjs_val_t result = MJS_OK;
    //Get function params
    mjs_val_t this = mjs_get_this(mjs);    //this interface object
    mjs_val_t client = mjs_get(mjs, this, SYS_PROP_CLIENT, ~0); //MQTT Client
    if (mjs_is_object(this) && mjs_is_foreign(client)) {
        esp_mqtt_client_reconnect(mjs_get_ptr(mjs, client));
    } else {
        mjs_set_errorf(mjs, MJS_INTERNAL_ERROR, "%s/%s: Incorrect params of function reconnect",
                       pcTaskGetTaskName(NULL), TAG_MQTT);
        result = MJS_INTERNAL_ERROR;
    }
    mjs_return(mjs, result);
}

mjs_val_t thingjsMQTTConstructor(struct mjs *mjs, cJSON *params) {
    //Create mjs object
    mjs_val_t interface = mjs_mk_object(mjs);

    //Bind functions
    stdi_setProtectedProperty(mjs, interface, "connect",
                              mjs_mk_foreign_func(mjs, (mjs_func_ptr_t) thingjsMQTTConnect));
    stdi_setProtectedProperty(mjs, interface, "reconnect",
                              mjs_mk_foreign_func(mjs, (mjs_func_ptr_t) thingjsMQTTReconnect));
    stdi_setProtectedProperty(mjs, interface, "subscribe",
                              mjs_mk_foreign_func(mjs, (mjs_func_ptr_t) thingjsMQTTSubscribe));
    stdi_setProtectedProperty(mjs, interface, "unsubscribe",
                              mjs_mk_foreign_func(mjs, (mjs_func_ptr_t) thingjsMQTTUnsubscribe));
    stdi_setProtectedProperty(mjs, interface, "publish",
                              mjs_mk_foreign_func(mjs, (mjs_func_ptr_t) thingjsMQTTPublish));
    //Return mJS interface object
    return interface;
}

void thingjsMQTTDestructor(struct mjs *mjs, mjs_val_t subject) {
    mjs_val_t mjs_client = mjs_get(mjs, subject, SYS_PROP_CLIENT, ~0);
    if (mjs_is_foreign(mjs_client)) {
        esp_mqtt_client_handle_t client = mjs_get_ptr(mjs, mjs_client);
        esp_mqtt_client_stop(client);
        esp_mqtt_client_destroy(client);
    }
}

void thingjsMQTTRegister(void) {
    static int thingjs_mqtt_cases[] = DEF_CASES(DEF_CASE(RES_VIRTUAL));

    static const struct st_thingjs_interface_manifest interface = {
            .type           = INTERFACE_NAME,
            .constructor    = thingjsMQTTConstructor,
            .destructor     = thingjsMQTTDestructor,
            .cases          = thingjs_mqtt_cases
    };

    thingjsRegisterInterface(&interface);
}
