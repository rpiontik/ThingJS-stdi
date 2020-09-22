//
// Created by rpiontik on 10.09.20.
//

#include "tgsi_mqttc.h"

#include "freertos/FreeRTOSConfig.h"
#include "freertos/FreeRTOS.h"
#include <esp_log.h>

#include "sdti_utils.h"
#include "thingjs_board.h"
#include "thingjs_core.h"
#include "mqtt_client.h"
#include "esp_tls.h"

#define  INTERFACE_NAME "mqttc"
#define  SYS_PROP_CONTEXT  "$context"

const char TAG_MQTT[] = INTERFACE_NAME;

struct thingjs_mqtt_context {
    esp_mqtt_client_handle_t client;
    TaskHandle_t process;
    mjs_val_t this;
    struct mjs *mjs;
};

#define MAX_NUMBER_EVENT_PARAMS 3

struct thingjs_mqtt_event {
    esp_mqtt_event_id_t event_id;
    int msg_id;
    struct thingjs_mqtt_context * context;
    union {
        struct {
            char * topic;
            int topic_len;
            char * payload;
            int payload_len;
        } data;
        struct {
            int mbedtls_err;
            esp_err_t err;
        } error;
    };
};

mjs_val_t thingjs_mqtt_event_callback(struct mjs *mjs, void *data) {
    struct thingjs_mqtt_event *callback_data = data;
    mjs_val_t result = MJS_OK;
    mjs_val_t args[MAX_NUMBER_EVENT_PARAMS] = {MJS_UNDEFINED};
    int agrs_num = 0;
    char *event_name = NULL;
    switch (callback_data->event_id) {
        case MQTT_EVENT_BEFORE_CONNECT:
            event_name = "onbeforeconnected";
            break;
        case MQTT_EVENT_CONNECTED:
            event_name = "onconnected";
            break;
        case MQTT_EVENT_DISCONNECTED:
            event_name = "ondisconnected";
            break;
        case MQTT_EVENT_SUBSCRIBED:
            event_name = "onsubscribed";
            agrs_num = 1;
            args[0] = mjs_mk_number(mjs, callback_data->msg_id);
            break;
        case MQTT_EVENT_UNSUBSCRIBED:
            event_name = "onunsubscribed";
            agrs_num = 1;
            args[0] = mjs_mk_number(mjs, callback_data->msg_id);
            break;
        case MQTT_EVENT_PUBLISHED:
            event_name = "onpublished";
            agrs_num = 1;
            args[0] = mjs_mk_number(mjs, callback_data->msg_id);
            break;
        case MQTT_EVENT_DATA:
            event_name = "ondata";
            agrs_num = 2;
            args[0] = mjs_mk_string(mjs,
                                    callback_data->data.topic,
                                    callback_data->data.topic_len,
                                    1
                                    );
            args[1] = mjs_mk_string(mjs,
                                    callback_data->data.payload,
                                    callback_data->data.payload_len,
                                    1
            );
            free(callback_data->data.payload);
            free(callback_data->data.topic);
            break;
        case MQTT_EVENT_ERROR:
            event_name = "onerror";
            agrs_num = 2;
            args[0] = mjs_mk_number(mjs, callback_data->error.err);
            args[1] = mjs_mk_number(mjs, callback_data->error.mbedtls_err);
            break;
        default:
            ESP_LOGE(TAG_MQTT, "Unknown event id:%d", callback_data->event_id);
            break;
    }

    if (event_name) {
        mjs_val_t func = mjs_get(mjs, callback_data->context->this, event_name, ~0);
        if (mjs_is_function(func)) {
            result = mjs_apply(mjs, NULL, func, callback_data->context->this, agrs_num, args);
        }
    }
    free(callback_data);
    return result;
}

static void thingjs_mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    esp_mqtt_event_handle_t event = event_data;
    struct thingjs_mqtt_event * callback_data = malloc(sizeof(struct thingjs_mqtt_event));
    callback_data->event_id = event->event_id;
    callback_data->msg_id = event->msg_id;
    callback_data->context = handler_args;

    switch (event->event_id) {
        case MQTT_EVENT_DATA:
            callback_data->data.payload = malloc(event->data_len);
            callback_data->data.payload_len = event->data_len;
            memcpy(callback_data->data.payload, event->data, event->data_len);

            callback_data->data.topic = malloc(event->topic_len);
            callback_data->data.topic_len = event->topic_len;
            memcpy(callback_data->data.topic, event->topic, event->topic_len);
            break;
        case MQTT_EVENT_ERROR:
            callback_data->error.err = esp_tls_get_and_clear_last_error(
                    event->error_handle, &callback_data->error.mbedtls_err, NULL);
            break;
        default:
            break;
    }

    if (pdTRUE != thingjsSendCallbackRequest(callback_data->context->process, thingjs_mqtt_event_callback, callback_data)) {
        free(callback_data);
        ESP_LOGE(TAG_MQTT, "Event stack is full! Event [%d] message [%d]", event->event_id, event->msg_id);
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
    mjs_val_t context = mjs_get(mjs, this, SYS_PROP_CONTEXT, ~0); //MQTT Client
    if (mjs_is_string(arg0) && mjs_is_string(arg1) && mjs_is_object(this) && mjs_is_foreign(context)) {
        int qos = mjs_is_number(arg2) ? mjs_get_int(mjs, arg2) : 0;
        int retain = mjs_is_number(arg3) ? mjs_get_int(mjs, arg3) : 0;
        size_t data_len = 0;
        const char * data = mjs_get_string(mjs, &arg1, &data_len);

        result = mjs_mk_number(mjs,
                               esp_mqtt_client_publish(
                                       ((struct thingjs_mqtt_context*)mjs_get_ptr(mjs,context))->client,
                                       mjs_get_cstring(mjs, &arg0),
                                       data,
                                       data_len,
                                       qos,
                                       retain
                               )
        );
    } else {
        mjs_set_errorf(mjs, MJS_INTERNAL_ERROR, "%s/%s: Incorrect params of function publish",
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
    mjs_val_t context = mjs_get(mjs, this, SYS_PROP_CONTEXT, ~0); //MQTT Client
    if (mjs_is_string(arg0) && mjs_is_object(this) && mjs_is_foreign(context)) {
        result = mjs_mk_number(mjs,
                               esp_mqtt_client_unsubscribe(
                                       ((struct thingjs_mqtt_context*)mjs_get_ptr(mjs,context))->client,
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
    mjs_val_t context = mjs_get(mjs, this, SYS_PROP_CONTEXT, ~0); //MQTT Client
    if (mjs_is_string(arg0) && mjs_is_object(this) && mjs_is_foreign(context)) {
        result = mjs_mk_number(mjs,
                               esp_mqtt_client_subscribe(
                                       ((struct thingjs_mqtt_context*)mjs_get_ptr(mjs,context))->client,
                                       mjs_get_cstring(mjs, &arg0),
                                       mjs_is_number(arg1)  ? mjs_get_int(mjs, arg1) : 0
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
    mjs_val_t this = mjs_get_this(mjs);    //this interface object
    if (mjs_is_string(arg0) && mjs_is_object(this)) {
        const esp_mqtt_client_config_t mqtt_cfg = {
                .uri = mjs_get_cstring(mjs, &arg0),
//                .cert_pem = (const char *) iot_eclipse_org_pem_start
        };

        esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
        if (client) {
            struct thingjs_mqtt_context *context = malloc(sizeof(struct thingjs_mqtt_context));
            context->client = client;
            context->process = xTaskGetCurrentTaskHandle();
            context->mjs = mjs;
            context->this = this;
            esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, thingjs_mqtt_event_handler, context);
            esp_mqtt_client_start(client);
            stdi_setProtectedProperty(mjs, this, SYS_PROP_CONTEXT, mjs_mk_foreign(mjs, context));

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
    mjs_val_t context = mjs_get(mjs, this, SYS_PROP_CONTEXT, ~0); //MQTT Client
    if (mjs_is_object(this) && mjs_is_foreign(context)) {
        esp_mqtt_client_reconnect(mjs_get_ptr(mjs, context));
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
    mjs_val_t mjs_context = mjs_get(mjs, subject, SYS_PROP_CONTEXT, ~0);
    if (mjs_is_foreign(mjs_context)) {
        struct thingjs_mqtt_context* context = mjs_get_ptr(mjs, mjs_context);
        esp_mqtt_client_stop(context->client);
        esp_mqtt_client_destroy(context->client);
        free(context);
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
