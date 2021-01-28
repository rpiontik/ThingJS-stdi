/*
 *  Created on: 26 nov. 2020 г.
 *      Author: nazguluz
 */

#include "tgsi_dac.h"
#include "driver/dac.h"
#include <esp_log.h>
#include "sdti_utils.h"
#include "thingjs_board.h"
#include "thingjs_core.h"

const char TAG_DAC[] = "DAC";

//
dac_channel_t getDacChannel(struct mjs *mjs) {
    //Get this object that store params
    mjs_val_t this_obj = mjs_get_this(mjs);
    //Get internal params
    uint32_t gpio = mjs_get_int32(mjs, mjs_get(mjs, this_obj, "dac", ~0));
    //return channel number
    ESP_LOGD(TAG_DAC, "GPIO: %d", gpio);
    if ( gpio == GPIO25 ) return DAC_CHANNEL_1;
    else if ( gpio == GPIO26) return DAC_CHANNEL_2;
    else {
        ESP_LOGE(TAG_DAC, "GPIO: %d", gpio);
        return DAC_CHANNEL_MAX;
    }
}

// Set DAC output voltage.
// DAC output is 8-bit. Maximum (255) corresponds to VDD.
// Note
// Need to configure DAC pad before calling this function.
// DAC channel 1 is attached to GPIO25, DAC channel 2 is attached to GPIO26
static void thingjsDacSetVoltage(struct mjs *mjs) {
    //Get function params
    mjs_val_t arg0 = mjs_arg(mjs, 0);
    //Param validation
    if (mjs_is_number(arg0)) {
        /* Set the DAC voltage */
        uint32_t val = mjs_get_int32(mjs, arg0);
        if (val > 255) {
            mjs_set_errorf(mjs, MJS_INTERNAL_ERROR, "%s: Incorrect value for function SetVoltage", TAG_DAC);
            mjs_return(mjs, MJS_INTERNAL_ERROR);
            return;
        }
        if (ESP_OK != dac_output_voltage(getDacChannel(mjs), (uint8_t)val )){
            mjs_set_errorf(mjs, MJS_INTERNAL_ERROR, "%s: Error to set value in function SetVoltage", TAG_DAC);
            mjs_return(mjs, MJS_INTERNAL_ERROR);
            return;
        }
    } else {
        mjs_set_errorf(mjs, MJS_INTERNAL_ERROR, "%s: Incorrect call function SetVoltage", TAG_DAC);
        mjs_return(mjs, MJS_INTERNAL_ERROR);
        return;
    }
    mjs_return(mjs, MJS_UNDEFINED);
}

//Interface function enable
static void thingjsDacEnable(struct mjs *mjs) {
    mjs_return(mjs, mjs_mk_boolean(mjs, dac_output_enable(getDacChannel(mjs)) == ESP_OK));
}

//Interface function disable
static void thingjsDacDisable(struct mjs *mjs) {
    mjs_return(mjs, mjs_mk_boolean(mjs, dac_output_disable(getDacChannel(mjs)) == ESP_OK));
}

mjs_val_t thingjsDacConstructor(struct mjs *mjs, cJSON *params) {
    //Validate preset params
    //The params must have pin number
    if (!cJSON_IsNumber(params))
        return MJS_UNDEFINED;

    //Get pin number
    gpio_num_t gpio = params->valueint;

    //Create mjs object
    mjs_val_t interface = mjs_mk_object(mjs);

    /* DAC pad output enable.
       Note:
       DAC channel 1 is attached to GPIO25, DAC channel 2 is attached to GPIO26
       I2S left channel will be mapped to DAC channel 2
       I2S right channel will be mapped to DAC channel 1
    */
    if (gpio == GPIO25) dac_output_enable(DAC_CHANNEL_1);
    else if (gpio == GPIO26) dac_output_enable(DAC_CHANNEL_2);

    //Add protected property to interface
    stdi_setProtectedProperty(mjs, interface, "dac", mjs_mk_number(mjs, gpio));

    //Bind functions
    stdi_setProtectedProperty(mjs, interface, "set",
            mjs_mk_foreign_func(mjs, (mjs_func_ptr_t) thingjsDacSetVoltage));
    stdi_setProtectedProperty(mjs, interface, "enable",
            mjs_mk_foreign_func(mjs, (mjs_func_ptr_t) thingjsDacEnable));
    stdi_setProtectedProperty(mjs, interface, "disable",
            mjs_mk_foreign_func(mjs, (mjs_func_ptr_t) thingjsDacDisable));

    //Return mJS interface object
    return interface;
}

void thingjsDacRegister(void) {
    static int thingjs_dac_cases[] = DEF_CASES(
            DEF_CASE(GPIO25), DEF_CASE(GPIO26)
    );

    static const struct st_thingjs_interface_manifest interface = {
            .type           = "dac",
            .constructor    = thingjsDacConstructor,
            .destructor     = NULL,
            .cases          = thingjs_dac_cases
    };

    thingjsRegisterInterface(&interface);
}



