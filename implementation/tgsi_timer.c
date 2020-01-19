//
// Created by rpiontik on 19.01.20.
//

#include <freertos/timers.h>
#include "tgsi_bit_port.h"

#include "sdti_utils.h"
#include "thingjs_board.h"
#include "thingjs_core.h"

const char TAG_TIMER[] = "TIMER";


struct timer_params {
    TimerHandle_t timer;
    bool is_interval;
    struct vm_process_params *process;
    mjs_val_t func;
    mjs_val_t interval;
    mjs_val_t param;
};

//Interface function direction
static void thingjsSetTimeout(struct mjs *mjs) {
    //Get function params
    mjs_val_t arg0 = mjs_arg(mjs, 0);   //Callback function
    mjs_val_t arg1 = mjs_arg(mjs, 1);   //Interval
    mjs_val_t arg3 = mjs_arg(mjs, 1);   //Params

    if (mjs_is_function(arg0) && mjs_is_number(arg1) && mjs_get_int32(mjs, arg1) > 0) {



    } else {
        mjs_set_errorf(mjs, MJS_INTERNAL_ERROR, "%s: Incorrect params of function setTimeout", TAG_TIMER);
        mjs_return(mjs, MJS_INTERNAL_ERROR);
        return;
    }

    mjs_return(mjs, MJS_UNDEFINED);
}

static void thingjsSetInterval(struct mjs *mjs) {
    //Get function params
    mjs_val_t arg0 = mjs_arg(mjs, 0);

    if (!mjs_is_function(arg0)) {

    }

    bool level;
    //Get target level
    if (mjs_is_boolean(arg0))
        level = mjs_get_bool(mjs, arg0) > 0;
    else if (mjs_is_number(arg0))
        level = mjs_get_int32(mjs, arg0) > 0;
    else {
        mjs_set_errorf(mjs, MJS_INTERNAL_ERROR, "%s: Incorrect internal params", TAG_TIMER);
        mjs_return(mjs, MJS_INTERNAL_ERROR);
        return;
    }

    mjs_return(mjs, MJS_UNDEFINED);
}


mjs_val_t thingjsBitPortConstructor(struct mjs *mjs, cJSON *params) {
    //Create mjs object
    mjs_val_t interface = mjs_mk_object(mjs);

    //Add protected property to interface
    mjs_set(mjs, interface, "gpio", ~0, mjs_mk_number(mjs, gpio));

    //Set protected flag
    mjs_set_protected(mjs, interface, "gpio", ~0, true);

    //Bind functions
    mjs_set(mjs, interface, "setTimeout", ~0,
            mjs_mk_foreign_func(mjs, (mjs_func_ptr_t) thingjsSetTimeout));
    mjs_set(mjs, interface, "setInterval", ~0,
            mjs_mk_foreign_func(mjs, (mjs_func_ptr_t) thingjsBitPortGet));

    //Consts
    mjs_set(mjs, interface, "DIR_MODE_DISABLE", ~0, mjs_mk_number(mjs, GPIO_MODE_DISABLE));
    mjs_set(mjs, interface, "DIR_MODE_DEF_INPUT", ~0, mjs_mk_number(mjs, GPIO_MODE_DEF_INPUT));
    mjs_set(mjs, interface, "DIR_MODE_DEF_OUTPUT", ~0, mjs_mk_number(mjs, GPIO_MODE_DEF_OUTPUT));
    mjs_set(mjs, interface, "DIR_MODE_INPUT_OUTPUT_OD", ~0, mjs_mk_number(mjs, GPIO_MODE_INPUT_OUTPUT_OD));
    mjs_set(mjs, interface, "DIR_MODE_INPUT_OUTPUT", ~0, mjs_mk_number(mjs, GPIO_MODE_INPUT_OUTPUT));

    //Return mJS interface object
    return interface;
}


void thingjsBitPortRegister(void) {
    static int thingjs_timer_cases[] = DEF_CASES(DEF_CASE(NON));

    static const struct st_thingjs_interface_manifest interface = {
            .type           = "timer",
            .constructor    = thingjsBitPortConstructor,
            .cases          = thingjs_timer_cases
    };

    thingjsRegisterInterface(&interface);
}
