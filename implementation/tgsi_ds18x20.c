//
// Created by rpiontik on 25.08.20.
//

#include "tgsi_ds18x20.h"

#include "freertos/FreeRTOSConfig.h"
#include "freertos/FreeRTOS.h"
#include <freertos/timers.h>
#include <esp_log.h>
#include <sys/time.h>

#include "sdti_utils.h"
#include "thingjs_board.h"
#include "thingjs_core.h"

#include "ow_driver.h"
#include "onewire.h"
#include "dallas.h"

#define  INTERFACE_NAME "DS18X20"

const char TAG_DS18X20[] = INTERFACE_NAME;
const char SYS_PROP_DRIVER[] = "$driver";


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

    ow_driver_ptr driver;
    init_driver(&driver, uart->valueint, rx->valueint, tx->valueint);
    stdi_setProtectedProperty(mjs, interface, SYS_PROP_DRIVER, mjs_mk_foreign(mjs, driver));

    owu_struct_t o2;
    owu_init(&o2, driver);

    //Bind functions
    /*
    stdi_setProtectedProperty(mjs, interface, "setTime",
            mjs_mk_foreign_func(mjs, (mjs_func_ptr_t) thingjsSetTime));
    stdi_setProtectedProperty(mjs, interface, "getTime",
            mjs_mk_foreign_func(mjs, (mjs_func_ptr_t) thingjsGetTime));
    */

    //Return mJS interface object
    return interface;
}

void thingjsDS18X20Destructor(struct mjs *mjs, mjs_val_t subject) {
    mjs_val_t driver = mjs_get(mjs, subject, SYS_PROP_DRIVER, ~0);

    if (mjs_is_foreign(driver)) {
        release_driver(mjs_get_ptr(mjs, driver));
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
