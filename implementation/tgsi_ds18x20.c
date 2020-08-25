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
#include "ow_driver_esp32_usart.h"
#include "onewire.h"
#include "dallas.h"

#define  INTERFACE_NAME "DS18X20"

const char TAG_DS18X20[] = INTERFACE_NAME;

mjs_val_t thingjsDS18X20Constructor(struct mjs *mjs, cJSON *params) {
    //Create mjs object
    mjs_val_t interface = mjs_mk_object(mjs);

    ow_driver_ptr driver;
    init_driver(&driver, E_UART1 | E_GPIO_RX_5 | E_GPIO_TX_4);

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

void thingjsDS18X20Register(void) {
    static int thingjs_DS18X20_cases[] = DEF_CASES(DEF_CASE(RES_VIRTUAL));

    static const struct st_thingjs_interface_manifest interface = {
            .type           = INTERFACE_NAME,
            .constructor    = thingjsDS18X20Constructor,
            .destructor     = NULL,
            .cases          = thingjs_DS18X20_cases
    };

    thingjsRegisterInterface(&interface);
}
