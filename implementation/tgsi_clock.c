//
// Created by rpiontik on 19.01.20.
//

#include "tgsi_timer.h"


#include "freertos/FreeRTOSConfig.h"
#include "freertos/FreeRTOS.h"
#include <freertos/timers.h>
#include <freertos/projdefs.h>
#include <freertos/queue.h>
#include <esp_log.h>
#include "tgsi_bit_port.h"
#include <sys/time.h>

#include "sdti_utils.h"
#include "thingjs_board.h"
#include "thingjs_core.h"

#define  INTERFACE_NAME "clock"

const char TAG_CLOCK[] = INTERFACE_NAME;

static void thingjsGetTime(struct mjs *mjs) {
    time_t now;
    time(&now);
    mjs_return(mjs, mjs_mk_number(mjs, now));
}

static void thingjsSetTime(struct mjs *mjs) {
    //Get function params
    mjs_val_t arg0 = mjs_arg(mjs, 0);   //Target time
    if (!mjs_is_number(arg0)){
        mjs_set_errorf(mjs, MJS_INTERNAL_ERROR, "%s: Incorrect setTime parameter", pcTaskGetTaskName(NULL));
        mjs_return(mjs, MJS_INTERNAL_ERROR);
    } else {

        struct timeval now = { .tv_sec = mjs_get_int32(mjs, arg0)};
        settimeofday(&now, NULL);

        thingjsGetTime(mjs);
    }
}

mjs_val_t thingjsClockConstructor(struct mjs *mjs, cJSON *params) {
    //Validate preset params
    //The params must have timer resource
    if (!cJSON_IsArray(params) || !(cJSON_GetArraySize(params) == 2)) {
        mjs_set_errorf(mjs, MJS_INTERNAL_ERROR, "%s: Incorrect params", TAG_CLOCK);
        mjs_return(mjs, MJS_INTERNAL_ERROR);
        return MJS_UNDEFINED;
    }

    cJSON * sda = cJSON_GetArrayItem(params, 0);
    cJSON * scl = cJSON_GetArrayItem(params, 1);

    if (!cJSON_IsNumber(sda) || !cJSON_IsNumber(scl) ) {
        mjs_set_errorf(mjs, MJS_INTERNAL_ERROR, "%s: Incorrect params", TAG_CLOCK);
        mjs_return(mjs, MJS_INTERNAL_ERROR);
        return MJS_UNDEFINED;
    }

    //Create mjs object
    mjs_val_t interface = mjs_mk_object(mjs);

    //Create timers collection
    stdi_setProtectedProperty(mjs, interface, "$sda", mjs_mk_number(mjs, sda->valueint));
    stdi_setProtectedProperty(mjs, interface, "$scl", mjs_mk_number(mjs, scl->valueint));

    //Bind functions
    stdi_setProtectedProperty(mjs, interface, "setTime",
            mjs_mk_foreign_func(mjs, (mjs_func_ptr_t) thingjsSetTime));
    stdi_setProtectedProperty(mjs, interface, "getTime",
            mjs_mk_foreign_func(mjs, (mjs_func_ptr_t) thingjsGetTime));

    //Return mJS interface object
    return interface;
}

void thingjsClockDestructor(struct mjs * mjs, mjs_val_t subject) {
}

void thingjsClockRegister(void) {
    static int thingjs_clock_cases[] = DEF_CASES(DEF_CASE(RES_VIRTUAL));

    static const struct st_thingjs_interface_manifest interface = {
            .type           = INTERFACE_NAME,
            .constructor    = thingjsClockConstructor,
            .destructor     = thingjsClockDestructor,
            .cases          = thingjs_clock_cases
    };

    thingjsRegisterInterface(&interface);
}
