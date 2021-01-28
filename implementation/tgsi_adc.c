/*
 *  Created on: 26 nov. 2020 г.
 *      Author: nazguluz
 */

#include "tgsi_adc.h"
#include "driver/adc.h"
#include <esp_log.h>
#include "sdti_utils.h"
#include "thingjs_board.h"
#include "thingjs_core.h"

const char TAG_ADC[] = "ADC";

esp_err_t setADC1Attn ( adc1_channel_t channel ,int attenj) {
    adc_atten_t atten = ADC_ATTEN_MAX;
    switch (attenj) {
        case ADC_ATTEN_DB_0:
            atten = ADC_ATTEN_DB_0;
            break;
        case ADC_ATTEN_DB_2_5:
            atten = ADC_ATTEN_DB_2_5;
            break;
        case ADC_ATTEN_DB_6:
            atten = ADC_ATTEN_DB_6;
            break;
        case ADC_ATTEN_DB_11:
            atten = ADC_ATTEN_DB_11;
            break;
        default:
            return ESP_ERR_INVALID_ARG;
    };
    return adc1_config_channel_atten( channel, atten );
}

esp_err_t setADC1Resolution ( int resj ) {
    adc_bits_width_t resolution = ADC_WIDTH_MAX;
    switch (resj) {
        case ADC_WIDTH_BIT_9:
            resolution = ADC_WIDTH_BIT_9;
            break;
        case ADC_WIDTH_BIT_10:
            resolution = ADC_WIDTH_BIT_10;
            break;
        case ADC_WIDTH_BIT_11:
            resolution = ADC_WIDTH_BIT_11;
            break;
        case ADC_WIDTH_BIT_12:
            resolution = ADC_WIDTH_BIT_12;
            break;
        default:
            return ESP_ERR_INVALID_ARG;
    };
    return adc1_config_width( resolution );
}

adc1_channel_t getAdc1ChannelFromGpio (gpio_num_t gpio) {
    switch (gpio) {
        case GPIO_NUM_36: // 36
            return ADC1_CHANNEL_0;
        case GPIO_NUM_37: // 37
            return ADC1_CHANNEL_1;
        case GPIO_NUM_38: // 38
            return ADC1_CHANNEL_2;
        case GPIO_NUM_39: // 39
            return ADC1_CHANNEL_3;
        case GPIO_NUM_32: // 32
            return ADC1_CHANNEL_4;
        case GPIO_NUM_33: // 33
            return ADC1_CHANNEL_5;
        case GPIO_NUM_34: // 34
            return ADC1_CHANNEL_6;
        case GPIO_NUM_35: // 35
            return ADC1_CHANNEL_7;
        default:
            return ADC1_CHANNEL_MAX;
    };
}

gpio_num_t getAdcGPIO(struct mjs *mjs) {
    //Get this object that store params
    mjs_val_t this_obj = mjs_get_this(mjs);
    //Get internal params
    return (gpio_num_t) mjs_get_int32(mjs, mjs_get(mjs, this_obj, "adc", ~0));
}

static void thingjsAdcGetRaw (struct mjs *mjs) {
    mjs_return(mjs, mjs_mk_number(mjs, adc1_get_raw( getAdc1ChannelFromGpio( getAdcGPIO(mjs)))));
}

// Set ADC1 config
// ADC_WIDTH_BIT
// ADC_ATTEN_DB
static void thingjsAdcSetConfig(struct mjs *mjs) {
    //Get function params
    mjs_val_t arg0 = mjs_arg(mjs, 0); // Resolution
    mjs_val_t arg1 = mjs_arg(mjs, 0); // Attenuation

    //Param validation
    if (mjs_is_number(arg0) || mjs_is_number(arg1)) {
        // Set resolution to ADC1
        if (ESP_OK != setADC1Resolution ( mjs_get_int(mjs, arg0) ) ) {
            mjs_set_errorf(mjs, MJS_INTERNAL_ERROR, "%s: Error to set resolution in AdcSetConfig", TAG_ADC);
            mjs_return(mjs, MJS_INTERNAL_ERROR);
            return;
        }
        // Set Attenuation to channel
        if (ESP_OK != setADC1Attn(getAdc1ChannelFromGpio(getAdcGPIO(mjs)),mjs_get_int(mjs, arg1)) ) {
            mjs_set_errorf(mjs, MJS_INTERNAL_ERROR, "%s: Error to set attenuation in AdcSetConfig", TAG_ADC);
            mjs_return(mjs, MJS_INTERNAL_ERROR);
            return;
        }
    } else {
        mjs_set_errorf(mjs, MJS_INTERNAL_ERROR, "%s: Incorrect call function AdcSetConfig", TAG_ADC);
        mjs_return(mjs, MJS_INTERNAL_ERROR);
        return;
    }
    mjs_return(mjs, MJS_UNDEFINED);
}

mjs_val_t thingjsAdcConstructor(struct mjs *mjs, cJSON *params) {
    //Validate preset params
    if (!cJSON_IsArray(params) || !(cJSON_GetArraySize(params) == 3)) {
        mjs_set_errorf(mjs, MJS_INTERNAL_ERROR, "%s: Incorrect params", TAG_ADC);
        mjs_return(mjs, MJS_INTERNAL_ERROR);
        return MJS_UNDEFINED;
    }

    cJSON * gpioj = cJSON_GetArrayItem(params, 0);
    cJSON * resolutionj = cJSON_GetArrayItem(params, 1);
    cJSON * attenj = cJSON_GetArrayItem(params, 2);

    if (!cJSON_IsNumber(gpioj) || !cJSON_IsNumber(resolutionj) || !cJSON_IsNumber(attenj) ) {
        mjs_set_errorf(mjs, MJS_INTERNAL_ERROR, "%s: Incorrect params", TAG_ADC);
        mjs_return(mjs, MJS_INTERNAL_ERROR);
        return MJS_UNDEFINED;
    }

    //Create mjs object
    mjs_val_t interface = mjs_mk_object(mjs);

    adc1_channel_t channel = getAdc1ChannelFromGpio(gpioj->valueint);

    /* ADC1 pad enable.
       Note:
       The ADC driver API supports ADC1 (8 channels, attached to GPIOs 32 - 39)
    */
    adc_power_on();
    if ( ESP_OK != adc_gpio_init( ADC_UNIT_1, channel ) ) {
        mjs_set_errorf(mjs, MJS_INTERNAL_ERROR, "%s: Incorrect GPIO param", TAG_ADC);
        mjs_return(mjs, MJS_INTERNAL_ERROR);
        return MJS_UNDEFINED;
    }
    if ( ESP_OK != setADC1Resolution ( resolutionj->valueint ) ) {
        mjs_set_errorf(mjs, MJS_INTERNAL_ERROR, "%s: Incorrect resolution param", TAG_ADC);
        mjs_return(mjs, MJS_INTERNAL_ERROR);
        return MJS_UNDEFINED;
    }
    if ( ESP_OK != setADC1Attn ( channel , attenj->valueint ) ) {
        mjs_set_errorf(mjs, MJS_INTERNAL_ERROR, "%s: Incorrect atten param", TAG_ADC);
        mjs_return(mjs, MJS_INTERNAL_ERROR);
        return MJS_UNDEFINED;
    }

    //Add protected property to interface
    stdi_setProtectedProperty(mjs, interface, "adc", mjs_mk_number(mjs, gpioj->valueint));

    //Bind functions
    stdi_setProtectedProperty(mjs, interface, "setConfig",
            mjs_mk_foreign_func(mjs, (mjs_func_ptr_t) thingjsAdcSetConfig));
    stdi_setProtectedProperty(mjs, interface, "getRaw",
            mjs_mk_foreign_func(mjs, (mjs_func_ptr_t) thingjsAdcGetRaw));

    //Consts
    stdi_setProtectedProperty(mjs, interface, "ADC_WIDTH_BIT_9",  mjs_mk_number(mjs, ADC_WIDTH_BIT_9 ));
    stdi_setProtectedProperty(mjs, interface, "ADC_WIDTH_BIT_10", mjs_mk_number(mjs, ADC_WIDTH_BIT_10));
    stdi_setProtectedProperty(mjs, interface, "ADC_WIDTH_BIT_11", mjs_mk_number(mjs, ADC_WIDTH_BIT_11));
    stdi_setProtectedProperty(mjs, interface, "ADC_WIDTH_BIT_12", mjs_mk_number(mjs, ADC_WIDTH_BIT_12));

    stdi_setProtectedProperty(mjs, interface, "ADC_ATTEN_DB_0", mjs_mk_number(mjs, ADC_ATTEN_DB_0));
    stdi_setProtectedProperty(mjs, interface, "ADC_ATTEN_DB_2_5", mjs_mk_number(mjs, ADC_ATTEN_DB_2_5));
    stdi_setProtectedProperty(mjs, interface, "ADC_ATTEN_DB_6", mjs_mk_number(mjs, ADC_ATTEN_DB_6));
    stdi_setProtectedProperty(mjs, interface, "ADC_ATTEN_DB_11", mjs_mk_number(mjs, ADC_ATTEN_DB_11));

    /* ADC_ATTEN_DB_0   = 0,  <The input voltage of ADC will be reduced to about 1/1 */
    /* ADC_ATTEN_DB_2_5 = 1,  <The input voltage of ADC will be reduced to about 1/1.34 */
    /* ADC_ATTEN_DB_6   = 2,  <The input voltage of ADC will be reduced to about 1/2 */
    /* ADC_ATTEN_DB_11  = 3,  <The input voltage of ADC will be reduced to about 1/3.6*/

    //Return mJS interface object
    return interface;
}

void thingjsAdcRegister(void) {
    static int thingjs_adc_cases[] = DEF_CASES(
            DEF_CASE(GPIO32), DEF_CASE(GPIO33), DEF_CASE(GPIO34), DEF_CASE(GPIO35), DEF_CASE(GPIO36), DEF_CASE(GPIO39)
    );

    static const struct st_thingjs_interface_manifest interface = {
            .type           = "adc",
            .constructor    = thingjsAdcConstructor,
            .destructor     = NULL,
            .cases          = thingjs_adc_cases
    };

    thingjsRegisterInterface(&interface);
}



