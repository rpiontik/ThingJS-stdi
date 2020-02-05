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

#include "sdti_utils.h"
#include "thingjs_board.h"
#include "thingjs_core.h"

#define  INTERFACE_NAME "timers"
#define  SYS_PROP_JOBS  "$jobs"

const char TAG_TIMER[] = INTERFACE_NAME;

struct timer_params {
    TaskHandle_t process;   //Context owner
    struct mjs *context;    //mJS object
    bool is_interval;       //Is interval timer
    mjs_val_t callback;     //mJS callback function
    mjs_val_t params;       //mJS callback params
};

void vm_timer_callback(TimerHandle_t xTimer) {
    struct timer_params *params = (struct timer_params *) pvTimerGetTimerID(xTimer);

    thingjsSyncCallMJSFunction(params->process, params->context, params->callback, params->params);

    if (!params->is_interval) {
        free(params);
        xTimerDelete(xTimer, 0);
    }
}

static void thingjsRunTimer(struct mjs *mjs, bool is_interval) {
    //Get function params
    mjs_val_t arg0 = mjs_arg(mjs, 0);   //Callback function
    mjs_val_t arg1 = mjs_arg(mjs, 1);   //Interval
    mjs_val_t arg3 = mjs_arg(mjs, 1);   //Params
    mjs_val_t this = mjs_get_this(mjs); //this interface object
    mjs_val_t jobs = mjs_get(mjs, this, SYS_PROP_JOBS, ~0); //Active timer's jobs

    const char *app_name = pcTaskGetTaskName(NULL);

    if (mjs_is_array(jobs) && mjs_is_object(this) && mjs_is_function(arg0)
        && mjs_is_number(arg1) && mjs_get_int32(mjs, arg1) > 0) {
        char timer_name[256];
        uint32_t interval = mjs_get_int32(mjs, arg1);
        snprintf(timer_name, sizeof(timer_name) - 1, "%s/%s/%d", app_name, TAG_TIMER, 0); //todo - need to ID of timer
        ESP_LOGD(TAG_TIMER, "Registered timer [%s]", timer_name);

        struct timer_params * params = malloc(sizeof(struct timer_params));
        params->process = xTaskGetCurrentTaskHandle();
        params->context = mjs;
        params->is_interval = is_interval;
        params->callback = arg0;
        params->params = arg3;


        TimerHandle_t timer_handle = xTimerCreate(timer_name, interval, is_interval ?  pdTRUE : pdFALSE,
                params, vm_timer_callback);

        if (xTimerStart(timer_handle, 0) != pdPASS) {
            xTimerDelete(timer_handle, 0);
            mjs_set_errorf(mjs, MJS_INTERNAL_ERROR, "Error of starting timer");
            mjs_return(mjs, MJS_INTERNAL_ERROR);
        } else {
            mjs_val_t timer_foreign = mjs_mk_foreign(mjs, timer_handle);
            mjs_array_push(mjs, jobs, timer_foreign);
            mjs_return(mjs, mjs_mk_foreign(mjs, timer_handle));
        }
    } else {
        mjs_set_errorf(mjs, MJS_INTERNAL_ERROR, "%s/%s: Incorrect params of function setTimeout", app_name, TAG_TIMER);
        mjs_return(mjs, MJS_INTERNAL_ERROR);
    }
}

static inline void thingjsSetTimeout(struct mjs *mjs) {
    thingjsRunTimer(mjs, false);
}

static inline void thingjsSetInterval(struct mjs *mjs) {
    thingjsRunTimer(mjs, true);
}

static inline void thingjsClearTimer(struct mjs *mjs) {
    //Get function params
    mjs_val_t foreign = mjs_arg(mjs, 0);   //Timer foreign
    mjs_val_t this = mjs_get_this(mjs); //this interface object
    mjs_val_t jobs = mjs_get(mjs, this, SYS_PROP_JOBS, ~0); //Active timer's jobs

    if (mjs_is_foreign(foreign) && mjs_is_array(jobs) && mjs_is_object(this)) {
        //Remove timer from jobs
        for(long i = mjs_array_length(mjs, jobs) - 1; i >= 0; i--) {
            if(mjs_array_get(mjs, jobs, i) == foreign) {
                mjs_array_del(mjs, jobs, i);
                break;
            }
        }
        //Delete timer from system
        if(xTimerDelete(mjs_get_ptr(mjs, foreign), 0) == pdPASS) {
            mjs_return(mjs, MJS_OK);
        } else {
            mjs_set_errorf(mjs, MJS_INTERNAL_ERROR, "%s: Can not clear timer", TAG_TIMER);
            mjs_return(mjs, MJS_INTERNAL_ERROR);
        }
    } else {
        mjs_set_errorf(mjs, MJS_INTERNAL_ERROR, "%s: Incorrect internal params", TAG_TIMER);
        mjs_return(mjs, MJS_INTERNAL_ERROR);
    }

    thingjsRunTimer(mjs, false);
}

mjs_val_t thingjsTimersConstructor(struct mjs *mjs, cJSON *params) {
    //Validate preset params
    //The params must have timer resource
    if (!cJSON_IsNumber(params) || (params->valueint != RES_TIMER)) {
        mjs_set_errorf(mjs, MJS_INTERNAL_ERROR, "%s: Incorrect params", TAG_TIMER);
        mjs_return(mjs, MJS_INTERNAL_ERROR);
        return MJS_UNDEFINED;
    }

    //Create mjs object
    mjs_val_t interface = mjs_mk_object(mjs);

    //Create timers collection
    stdi_setProtectedProperty(mjs, interface, SYS_PROP_JOBS, mjs_mk_array(mjs));

    //Bind functions
    stdi_setProtectedProperty(mjs, interface, "setTimeout",
            mjs_mk_foreign_func(mjs, (mjs_func_ptr_t) thingjsSetTimeout));
    stdi_setProtectedProperty(mjs, interface, "setInterval",
            mjs_mk_foreign_func(mjs, (mjs_func_ptr_t) thingjsSetInterval));
    stdi_setProtectedProperty(mjs, interface, "setInterval",
                              mjs_mk_foreign_func(mjs, (mjs_func_ptr_t) thingjsSetInterval));
    stdi_setProtectedProperty(mjs, interface, "clearInterval",
                              mjs_mk_foreign_func(mjs, (mjs_func_ptr_t) thingjsClearTimer));
    stdi_setProtectedProperty(mjs, interface, "clearTimeout",
                              mjs_mk_foreign_func(mjs, (mjs_func_ptr_t) thingjsClearTimer));

    //Return mJS interface object
    return interface;
}

void thingjsTimersDestructor(struct mjs * mjs, mjs_val_t subject) {
    mjs_val_t jobs = mjs_get(mjs, subject, SYS_PROP_JOBS, ~0); //Active timer's jobs

    if (mjs_is_array(jobs)) {
        //Remove timer from jobs
        for(long i = mjs_array_length(mjs, jobs) - 1; i >= 0; i--) {
            xTimerDelete(mjs_get_ptr(mjs, mjs_array_get(mjs, jobs, i)), 0);
        }
    } else {
        ESP_LOGE(TAG_TIMER, "Fatal error of timer destructor");
    }
}

void thingjsTimersRegister(void) {
    static int thingjs_timer_cases[] = DEF_CASES(DEF_CASE(RES_VIRTUAL));


    static const struct st_thingjs_interface_manifest interface = {
            .type           = INTERFACE_NAME,
            .constructor    = thingjsTimersConstructor,
            .destructor     = thingjsTimersDestructor,
            .cases          = thingjs_timer_cases
    };

    thingjsRegisterInterface(&interface);
}
