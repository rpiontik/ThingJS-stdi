//
// Created by rpiontik on 25.08.20.
//

#include "tgsi_ds18x20.h"

#include "freertos/FreeRTOSConfig.h"
#include "freertos/FreeRTOS.h"
#include <freertos/timers.h>
#include <esp_log.h>
#include <sys/time.h>
#include <string.h>
#include "driver/uart.h"

#include "sdti_utils.h"
#include "thingjs_board.h"
#include "thingjs_core.h"

#include "ow_driver.h"
#include "onewire.h"
#include "dallas.h"

#define  INTERFACE_NAME "DS18X20"

#define MOC_DATA

const char TAG_DS18X20[] = INTERFACE_NAME;

const char SYS_PROP_CONEXT[]    = "$context";

const char FUNC_CONVERT_ALL[]   = "convert_all";
const char FUNC_GET_TEMP_C[]    = "get_temp_c";
const char FUNC_SEARCH[]        = "search";

const char ERR_INCORRECT_PARAMS[] = "Incorrect params of function";

static void thingjsDS18X20GetTempC(struct mjs *mjs) {
    mjs_val_t result;
    //Get function params
    mjs_val_t addr = mjs_arg(mjs, 0);   //Address
    mjs_val_t this = mjs_get_this(mjs); //this interface object
    mjs_val_t context = mjs_get(mjs, this, SYS_PROP_CONEXT, ~0);

    if (mjs_is_object(this) && mjs_is_foreign(context) && mjs_is_string(addr)) {
        uint8_t dev_addr[8] = {0};
        memcpy(dev_addr, mjs_get_string(mjs, &addr, NULL), 8);
#ifdef MOC_DATA
        ESP_LOGD(TAG_DS18X20, "Address [%d,%d,%d,%d,%d,%d,%d,%d]", dev_addr[0], dev_addr[1], dev_addr[2], dev_addr[3],
                 dev_addr[4], dev_addr[5], dev_addr[6], dev_addr[7]);
        result = mjs_mk_number(mjs, 32.23);
#else
        owu_struct_t * wire = mjs_get_ptr(mjs, context);
        uint8_t scratch_pad[__SCR_LENGTH];
        ds_read_scratchpad(wire, dev_addr, scratch_pad);
        result = mjs_mk_number(mjs, ds_get_temp_c(scratch_pad));
#endif
    } else {
        mjs_set_errorf(mjs, MJS_INTERNAL_ERROR, "%s/%s: %s %s",
                       pcTaskGetTaskName(NULL), TAG_DS18X20, ERR_INCORRECT_PARAMS, FUNC_GET_TEMP_C);
        result = MJS_INTERNAL_ERROR;
    }
    mjs_return(mjs, result);
}


static void thingjsDS18X20ConvertAll(struct mjs *mjs) {
    //Get function params
    mjs_val_t this = mjs_get_this(mjs); //this interface object
    mjs_val_t context = mjs_get(mjs, this, SYS_PROP_CONEXT, ~0);

    if (mjs_is_object(this) && mjs_is_foreign(context)) {
#ifndef MOC_DATA
        ds_convert_all(mjs_get_ptr(mjs, context));
#endif
        mjs_return(mjs, MJS_OK);
    } else {
        mjs_return(mjs, MJS_INTERNAL_ERROR);
    }
}

static void thingjsDS18X20Search(struct mjs *mjs) {
    mjs_val_t result = MJS_OK;
    //Get function params
    mjs_val_t func = mjs_arg(mjs, 0);   //Callback function
    mjs_val_t this = mjs_get_this(mjs);    //this interface object
    mjs_val_t context = mjs_get(mjs, this, SYS_PROP_CONEXT, ~0);

    if (mjs_is_object(this) && mjs_is_function(func) && mjs_is_foreign(context)) {
        owu_struct_t * wire = mjs_get_ptr(mjs, context);
        owu_reset_search(wire);
#ifdef MOC_DATA
        uint8_t dev_addr[8] = {1, 2, 3, 4, 5, 6, 7, 8};
        mjs_val_t addr = mjs_mk_string(mjs, (char *)dev_addr, 8, true);
        result = mjs_apply(mjs, NULL, func, MJS_UNDEFINED, 1, &addr);
#elif
        uint8_t dev_addr[8];
        while((result == MJS_OK) && owu_search(wire, dev_addr)) {
            mjs_val_t addr = mjs_mk_string(mjs, (char *)dev_addr, 8, true);
            result = mjs_apply(mjs, NULL, func, MJS_UNDEFINED, 1, &addr);
        }
#endif
    } else {
        mjs_set_errorf(mjs, MJS_INTERNAL_ERROR, "%s/%s: %s %s",
                       pcTaskGetTaskName(NULL), TAG_DS18X20, ERR_INCORRECT_PARAMS, FUNC_SEARCH);
        result = MJS_INTERNAL_ERROR;
    }
    mjs_return(mjs, result);
}

mjs_val_t thingjsDS18X20Constructor(struct mjs *mjs, cJSON *params) {
    //Validate preset params
    if (!cJSON_IsArray(params) || !(cJSON_GetArraySize(params) == 3)) {
        mjs_set_errorf(mjs, MJS_INTERNAL_ERROR, "%s: Incorrect params", TAG_DS18X20);
        mjs_return(mjs, MJS_INTERNAL_ERROR);
        return MJS_UNDEFINED;
    }

    cJSON * uart = cJSON_GetArrayItem(params, 0);
    cJSON * rx = cJSON_GetArrayItem(params, 1);
    cJSON * tx = cJSON_GetArrayItem(params, 2);

    if (!cJSON_IsNumber(uart) || !cJSON_IsNumber(rx) || !cJSON_IsNumber(tx)) {
        mjs_set_errorf(mjs, MJS_INTERNAL_ERROR, "%s: Incorrect params", TAG_DS18X20);
        mjs_return(mjs, MJS_INTERNAL_ERROR);
        return MJS_UNDEFINED;
    }

    //Create mjs object
    mjs_val_t interface = mjs_mk_object(mjs);

    owu_struct_t * context = malloc(sizeof(owu_struct_t));
    int uart_number = 0;
    switch (uart->valueint)
    {
        case RES_UART_0:
            uart_number = UART_NUM_0;
            break;
        case RES_UART_1:
            uart_number = UART_NUM_1;
            break;
        case RES_UART_2:
            uart_number = UART_NUM_2;
            break;
    }
    init_driver(&context->driver, uart_number, rx->valueint, tx->valueint);
    stdi_setProtectedProperty(mjs, interface, SYS_PROP_CONEXT, mjs_mk_foreign(mjs, context));

    //Bind functions
    stdi_setProtectedProperty(mjs, interface, FUNC_SEARCH,
            mjs_mk_foreign_func(mjs, (mjs_func_ptr_t) thingjsDS18X20Search));
    stdi_setProtectedProperty(mjs, interface, FUNC_CONVERT_ALL,
            mjs_mk_foreign_func(mjs, (mjs_func_ptr_t) thingjsDS18X20ConvertAll));
    stdi_setProtectedProperty(mjs, interface, FUNC_GET_TEMP_C,
                              mjs_mk_foreign_func(mjs, (mjs_func_ptr_t) thingjsDS18X20GetTempC));

    //Return mJS interface object
    return interface;
}

void thingjsDS18X20Destructor(struct mjs *mjs, mjs_val_t subject) {
    mjs_val_t context = mjs_get(mjs, subject, SYS_PROP_CONEXT, ~0);
    if (mjs_is_foreign(context)) {
        owu_struct_t * wire = mjs_get_ptr(mjs, context);
        release_driver(&wire->driver);
        free(wire);
    }
}

void thingjsDS18X20Register(void) {
    static int thingjs_DS18X20_cases[] = DEF_CASES(
            DEF_CASE(
                    DEF_ENUM(RES_UART_0, RES_UART_1, RES_UART_2),
                    //RX
                    DEF_ENUM(
                            GPIO2, GPIO4, GPIO5, GPIO13, GPIO14, GPIO15, GPIO16, GPIO17, GPIO18, GPIO19, GPIO21, GPIO22,
                            GPIO23, GPIO25, GPIO26, GPIO27, GPIO32, GPIO33, GPIO34, GPIO35, GPIO36, GPIO39
                    )
                    //TX
                    ,DEF_ENUM(
                            GPIO2, GPIO4, GPIO5, GPIO12, GPIO13, GPIO14, GPIO15, GPIO16, GPIO17, GPIO18, GPIO19, GPIO21,
                            GPIO22, GPIO23, GPIO25, GPIO26, GPIO27, GPIO32, GPIO33
                    )
            )
    );;

    static const struct st_thingjs_interface_manifest interface = {
            .type           = INTERFACE_NAME,
            .constructor    = thingjsDS18X20Constructor,
            .destructor     = thingjsDS18X20Destructor,
            .cases          = thingjs_DS18X20_cases
    };

    thingjsRegisterInterface(&interface);
}
