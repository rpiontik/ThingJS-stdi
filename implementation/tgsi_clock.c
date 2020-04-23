//
// Created by rpiontik on 19.01.20.
//

#include "tgsi_timer.h"


#include "freertos/FreeRTOSConfig.h"
#include "freertos/FreeRTOS.h"
#include <freertos/timers.h>
#include <esp_log.h>
#include <sys/time.h>

#include "sdti_utils.h"
#include "thingjs_board.h"
#include "thingjs_core.h"

#define  INTERFACE_NAME "clock"

const char TAG_CLOCK[] = INTERFACE_NAME;

static void thingjsGetTime(struct mjs *mjs) {
    time_t now = {0};
    currentTime(&now);
    mjs_return(mjs, mjs_mk_number(mjs, now));
}

static void thingjsSetTime(struct mjs *mjs) {
    //Get function params
    mjs_val_t arg0 = mjs_arg(mjs, 0);   //Target time
    if (!mjs_is_number(arg0)){
        mjs_set_errorf(mjs, MJS_INTERNAL_ERROR, "%s: Incorrect setTime parameter", pcTaskGetTaskName(NULL));
        mjs_return(mjs, MJS_INTERNAL_ERROR);
    } else {
        setCurrentTime(mjs_get_int32(mjs, arg0));
        thingjsGetTime(mjs);
    }
}

mjs_val_t thingjsClockConstructor(struct mjs *mjs, cJSON *params) {
    //Create mjs object
    mjs_val_t interface = mjs_mk_object(mjs);

    //Bind functions
    stdi_setProtectedProperty(mjs, interface, "setTime",
            mjs_mk_foreign_func(mjs, (mjs_func_ptr_t) thingjsSetTime));
    stdi_setProtectedProperty(mjs, interface, "getTime",
            mjs_mk_foreign_func(mjs, (mjs_func_ptr_t) thingjsGetTime));

    //Return mJS interface object
    return interface;
}

void thingjsClockRegister(void) {
    static int thingjs_clock_cases[] = DEF_CASES(DEF_CASE(RES_VIRTUAL));

    static const struct st_thingjs_interface_manifest interface = {
            .type           = INTERFACE_NAME,
            .constructor    = thingjsClockConstructor,
            .destructor     = NULL,
            .cases          = thingjs_clock_cases
    };

    thingjsRegisterInterface(&interface);
}
