/*
 *  Created on: 21 мар. 2019 г.
 *      Author: rpiontik
 */

#include "tgsi_bit_port.h"

#include "driver/gpio.h"

#include "sdti_utils.h"
#include "thingjs_board.h"
#include "thingjs_core.h"

const char TAG_BIT_PORT[] = "BIT_PORT";

//
gpio_num_t getGPIO(struct mjs *mjs) {
    //Get this object that store params
    mjs_val_t this_obj = mjs_get_this(mjs);
    //Get internal params
    return (gpio_num_t) mjs_get_int32(mjs, mjs_get(mjs, this_obj, "gpio", ~0));
}

//Interface function direction
static void thingjsBitPortSet(struct mjs *mjs) {
    //Get function params
    mjs_val_t arg0 = mjs_arg(mjs, 0);

    bool level;
    //Get target level
    if (mjs_is_boolean(arg0))
        level = mjs_get_bool(mjs, arg0) > 0;
    else if (mjs_is_number(arg0))
        level = mjs_get_int32(mjs, arg0) > 0;
    else {
        mjs_set_errorf(mjs, MJS_INTERNAL_ERROR, "%s: Incorrect internal params", TAG_BIT_PORT);
        mjs_return(mjs, MJS_INTERNAL_ERROR);
        return;
    }

    gpio_set_level(getGPIO(mjs), level ? 1 : 0);

    mjs_return(mjs, MJS_UNDEFINED);
}


//Interface function set
static void thingjsBitPortDirection(struct mjs *mjs) {
    //Get function params
    mjs_val_t arg0 = mjs_arg(mjs, 0);
    //Param validation
    if (mjs_is_number(arg0)) {
        /* Set the GPIO as a push/pull output */
        gpio_set_direction(getGPIO(mjs), (gpio_mode_t) mjs_get_int32(mjs, arg0));
    } else {
        mjs_set_errorf(mjs, MJS_INTERNAL_ERROR, "%s: Incorrect call function direction(mode)", TAG_BIT_PORT);
        mjs_return(mjs, MJS_INTERNAL_ERROR);
        return;
    }
    mjs_return(mjs, MJS_UNDEFINED);
}

//Interface function get
static void thingjsBitPortGet(struct mjs *mjs) {
    mjs_return(mjs, mjs_mk_boolean(mjs, gpio_get_level(getGPIO(mjs)) != 0));
}

mjs_val_t thingjsBitPortConstructor(struct mjs *mjs, cJSON *params) {
    //Validate preset params
    //The params must have pin number
    if (!cJSON_IsNumber(params))
        return MJS_UNDEFINED;

    //Get pin number
    gpio_num_t gpio = params->valueint;

    //Create mjs object
    mjs_val_t interface = mjs_mk_object(mjs);

    /* Configure the IOMUX register for pad BLINK_GPIO (some pads are
       muxed to GPIO on reset already, but some default to other
       functions and need to be switched to GPIO. Consult the
       Technical Reference for a list of pads and their default
       functions.)
    */
    gpio_pad_select_gpio(gpio);

    //Add protected property to interface
    stdi_setProtectedProperty(mjs, interface, "gpio", mjs_mk_number(mjs, gpio));

    //Bind functions
    stdi_setProtectedProperty(mjs, interface, "set",
            mjs_mk_foreign_func(mjs, (mjs_func_ptr_t) thingjsBitPortSet));
    stdi_setProtectedProperty(mjs, interface, "get",
            mjs_mk_foreign_func(mjs, (mjs_func_ptr_t) thingjsBitPortGet));
    stdi_setProtectedProperty(mjs, interface, "direction",
            mjs_mk_foreign_func(mjs, (mjs_func_ptr_t) thingjsBitPortDirection));

    //Consts
    stdi_setProtectedProperty(mjs, interface, "DIR_MODE_DISABLE", mjs_mk_number(mjs, GPIO_MODE_DISABLE));
    stdi_setProtectedProperty(mjs, interface, "DIR_MODE_INPUT", mjs_mk_number(mjs, GPIO_MODE_INPUT));
    stdi_setProtectedProperty(mjs, interface, "DIR_MODE_OUTPUT", mjs_mk_number(mjs, GPIO_MODE_OUTPUT));
    stdi_setProtectedProperty(mjs, interface, "DIR_MODE_OUTPUT_OD", mjs_mk_number(mjs, GPIO_MODE_OUTPUT_OD));
    stdi_setProtectedProperty(mjs, interface, "DIR_MODE_INPUT_OUTPUT_OD", mjs_mk_number(mjs, GPIO_MODE_INPUT_OUTPUT_OD));
    stdi_setProtectedProperty(mjs, interface, "DIR_MODE_INPUT_OUTPUT", mjs_mk_number(mjs, GPIO_MODE_INPUT_OUTPUT));

    //Return mJS interface object
    return interface;
}

void thingjsBitPortRegister(void) {
    static int thingjs_bit_port_cases[] = DEF_CASES(
            DEF_CASE(GPIO0),  DEF_CASE(GPIO2), DEF_CASE(GPIO3), DEF_CASE(GPIO4),
            DEF_CASE(GPIO5),  DEF_CASE(GPIO12), DEF_CASE(GPIO13), DEF_CASE(GPIO14),
            DEF_CASE(GPIO15), DEF_CASE(GPIO16), DEF_CASE(GPIO17), DEF_CASE(GPIO18),
            DEF_CASE(GPIO19), DEF_CASE(GPIO21), DEF_CASE(GPIO22), DEF_CASE(GPIO23),
            DEF_CASE(GPIO25), DEF_CASE(GPIO26), DEF_CASE(GPIO27), DEF_CASE(GPIO32),
            DEF_CASE(GPIO33)
    );

    static const struct st_thingjs_interface_manifest interface = {
            .type           = "bit_port",
            .constructor    = thingjsBitPortConstructor,
            .destructor     = NULL,
            .cases          = thingjs_bit_port_cases
    };

    thingjsRegisterInterface(&interface);
}



