//
// Created by rpiontik on 19.01.20.
//

#include "freertos/FreeRTOSConfig.h"
#include "freertos/FreeRTOS.h"
#include <freertos/timers.h>
#include <freertos/projdefs.h>
#include <freertos/queue.h>
#include "tgsi_bit_port.h"

#include "sdti_utils.h"
#include "thingjs_board.h"
#include "thingjs_core.h"

const char INTERFACE_NAME[] = "timer";

struct timer_params {
    TimerHandle_t timer;    //Handle of timer
    struct mjs *mjs;        //mJS process
    bool is_interval;       //Is interval timer
    mjs_val_t callback;     //mJS callback function
    mjs_val_t params;       //mJS callback params
};

void vm_timer_callback(TimerHandle_t xTimer) {
    struct timer_params *params = (struct timer_params *) pvTimerGetTimerID(xTimer);

    struct ubus_message ubm = {
            .type = ubus_timer
    };

    memcpy(&ubm.data, params, sizeof(struct vm_timer_params));
    xQueueSend(params->process->input, &ubm, 1000);

    if (!params->interval) {
        xTimerDelete(xTimer, 0);
        params->timer = 0;
    }
}

static void thingjsRunTimer(struct mjs *mjs, bool is_interval) {
    //Get function params
    mjs_val_t arg0 = mjs_arg(mjs, 0);   //Callback function
    mjs_val_t arg1 = mjs_arg(mjs, 1);   //Interval
    mjs_val_t arg3 = mjs_arg(mjs, 1);   //Params

    const char *app_name = pcTaskGetTaskName(NULL);

    if (mjs_is_function(arg0) && mjs_is_number(arg1) && mjs_get_int32(mjs, arg1) > 0) {
        char timer_name[256];
        uint32_t interval = mjs_get_int32(mjs, arg1);
        snprintf(timer_name, sizeof(timer_name) - 1, "%s/%s/%d", app_name, INTERFACE_NAME, 0); //todo - need to ID of timer

        struct timer_params * params = malloc(sizeof(struct timer_params));
        params->mjs = mjs;
        params->is_interval = is_interval;
        params->callback = arg0;
        params->params = arg3;

        TimerHandle_t timer_handle = xTimerCreate(timer_name, interval, is_interval ?  pdTRUE : pdFALSE,
                params, vm_timer_callback);

        mjs_return(mjs, mjs_mk_foreign(mjs, timer_handle));
    } else {
        mjs_set_errorf(mjs, MJS_INTERNAL_ERROR, "%s/%s: Incorrect params of function setTimeout", app_name, INTERFACE_NAME);
        mjs_return(mjs, MJS_INTERNAL_ERROR);
    }
}

static inline void thingjsSetTimeout(struct mjs *mjs) {
    thingjsRunTimer(mjs, true);
}

static inline void thingjsSetInterval(struct mjs *mjs) {
    thingjsRunTimer(mjs, false);
}


mjs_val_t thingjsTimerConstructor(struct mjs *mjs, cJSON *params) {
    //Create mjs object
    mjs_val_t interface = mjs_mk_object(mjs);

    //Bind functions
    stdi_setProtectedProperty(mjs, interface, "setTimeout",
            mjs_mk_foreign_func(mjs, (mjs_func_ptr_t) thingjsSetTimeout));
    stdi_setProtectedProperty(mjs, interface, "setInterval",
            mjs_mk_foreign_func(mjs, (mjs_func_ptr_t) thingjsSetInterval));

    //Return mJS interface object
    return interface;
}


void thingjsBitPortRegister(void) {
    static int thingjs_timer_cases[] = DEF_CASES(DEF_CASE(NON));

    static const struct st_thingjs_interface_manifest interface = {
            .type           = INTERFACE_NAME,
            .constructor    = thingjsTimerConstructor,
            .cases          = thingjs_timer_cases
    };

    thingjsRegisterInterface(&interface);
}
