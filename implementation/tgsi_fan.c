/*
 *  Created on: 28 nov. 2020 г.
 *      Author: nazguluz
 */

#include "tgsi_fan.h"
#include "driver/dac.h"
#include "driver/adc.h"
#include "driver/pcnt.h"
#include <esp_log.h>
#include "sdti_utils.h"
#include "thingjs_board.h"
#include "thingjs_core.h"

const char TAG_FAN[] = "FAN";

#define ADC_UNIT  ADC_UNIT_1
#define ADC_WIDTH ADC_WIDTH_BIT_12
#define ADC_ATTEN ADC_ATTEN_DB_11

#define PCNT_UNIT PCNT_UNIT_0
#define PCNT_CHANNEL PCNT_CHANNEL_0
#define PCNT_H_LIM_VAL 32767
#define PCNT_L_LIM_VAL -10
#define PCNT_FILTER_VALUE 5

typedef struct {
    dac_channel_t   dacChannel;
    gpio_num_t      dacGpio;
    adc1_channel_t  adcChannel;
    gpio_num_t      adcGpio;
    gpio_num_t      pcntGpio;
} st_fan_config;

static st_fan_config fanConfig = {
    .dacChannel = DAC_CHANNEL_MAX,
    .dacGpio    = GPIO_NUM_NC,
    .adcChannel = ADC1_CHANNEL_MAX,
    .adcGpio    = GPIO_NUM_NC,
    .pcntGpio   = GPIO_NUM_NC
};

//
dac_channel_t getDacChannel(struct mjs *mjs) {
    //Get this object that store params
    mjs_val_t this_obj = mjs_get_this(mjs);
    //Get internal params
    uint32_t gpio = mjs_get_int32(mjs, mjs_get(mjs, this_obj, "dac", ~0));
    //return channel number
    if ( gpio == DAC_CHANNEL_1_GPIO_NUM ) return DAC_CHANNEL_1;
    else if ( gpio == DAC_CHANNEL_2_GPIO_NUM) return DAC_CHANNEL_2;
    else {
        ESP_LOGE(TAG_FAN, "GPIO: %d doesn't support DAC", gpio);
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

/* Initialize PCNT functions:
 *  - configure and initialize PCNT
 *  - set up the input filter
 *  - set up the counter events to watch
 */
static esp_err_t pcnt_init(gpio_num_t gpio) {
    /* Prepare configuration for the PCNT unit */
    pcnt_config_t pcnt_config = {
            // Set PCNT input signal and control GPIOs
            .pulse_gpio_num = gpio,
            .ctrl_gpio_num = -1,
            .channel = PCNT_CHANNEL,
            .unit = PCNT_UNIT,
            // What to do on the positive / negative edge of pulse input?
            .pos_mode = PCNT_COUNT_INC,   // Count up on the positive edge
            .neg_mode = PCNT_COUNT_DIS,   // Keep the counter value on the negative edge
            // What to do when control input is low or high?
            .lctrl_mode = PCNT_MODE_KEEP, // Reverse counting direction if low
            .hctrl_mode = PCNT_MODE_KEEP,    // Keep the primary counter mode if high
            // Set the maximum and minimum limit values to watch
            .counter_h_lim = PCNT_H_LIM_VAL,
            .counter_l_lim = PCNT_L_LIM_VAL,
    };
    /* Initialize PCNT unit */
    if (ESP_OK != pcnt_unit_config(&pcnt_config)) return ESP_ERR_INVALID_ARG;

    /* Configure and enable the input filter */
    pcnt_set_filter_value(PCNT_UNIT, PCNT_FILTER_VALUE);
    pcnt_filter_enable(PCNT_UNIT);

    /* Set threshold 0 and 1 values and enable events to watch */
    //pcnt_set_event_value(PCNT_UNIT, PCNT_EVT_THRES_1, PCNT_THRESH1_VAL);
    //pcnt_event_enable(PCNT_UNIT, PCNT_EVT_THRES_1);
    //pcnt_set_event_value(PCNT_UNIT, PCNT_EVT_THRES_0, PCNT_THRESH0_VAL);
    //pcnt_event_enable(PCNT_UNIT, PCNT_EVT_THRES_0);
    /* Enable events on zero, maximum and minimum limit values */
    //pcnt_event_enable(PCNT_UNIT, PCNT_EVT_ZERO);
    //pcnt_event_enable(PCNT_UNIT, PCNT_EVT_H_LIM);
    //pcnt_event_enable(PCNT_UNIT, PCNT_EVT_L_LIM);

    /* Initialize PCNT's counter */
    pcnt_counter_pause(PCNT_UNIT);
    pcnt_counter_clear(PCNT_UNIT);

    /* Register ISR handler and enable interrupts for PCNT unit */
    //pcnt_isr_register(pcnt_example_intr_handler, NULL, 0, &user_isr_handle);
    //pcnt_intr_enable(PCNT_TEST_UNIT);

    /* Everything is set up, now go to counting */
    pcnt_counter_resume(PCNT_UNIT);

    return ESP_OK;
}

mjs_val_t thingjsFanConstructor(struct mjs *mjs, cJSON *params) {
    //Validate preset params
    if (!cJSON_IsArray(params) || !(cJSON_GetArraySize(params) == 3)) {
        mjs_set_errorf(mjs, MJS_INTERNAL_ERROR, "%s: Incorrect params", TAG_FAN);
        mjs_return(mjs, MJS_INTERNAL_ERROR);
        return MJS_UNDEFINED;
    }

    cJSON * gpioDacj = cJSON_GetArrayItem(params, 0);
    cJSON * gpioAdcj = cJSON_GetArrayItem(params, 1);
    cJSON * gpioCntj = cJSON_GetArrayItem(params, 2);

    if (!cJSON_IsNumber(gpioDacj) || !cJSON_IsNumber(gpioAdcj) || !cJSON_IsNumber(gpioCntj) ) {
        mjs_set_errorf(mjs, MJS_INTERNAL_ERROR, "%s: Incorrect GPIO params", TAG_FAN);
        mjs_return(mjs, MJS_INTERNAL_ERROR);
        return MJS_UNDEFINED;
    }

    //Create mjs object
    mjs_val_t interface = mjs_mk_object(mjs);

    /* DAC pad output enable.
       Note:
       DAC channel 1 is attached to GPIO25, DAC channel 2 is attached to GPIO26
       I2S left channel will be mapped to DAC channel 2
       I2S right channel will be mapped to DAC channel 1
    */
    switch ( (gpio_num_t) gpioDacj->valueint ) {
        case DAC_CHANNEL_1_GPIO_NUM:
            fanConfig.dacGpio = DAC_CHANNEL_1_GPIO_NUM;
            fanConfig.dacChannel = DAC_CHANNEL_1;
            break;
        case DAC_CHANNEL_2_GPIO_NUM:
            fanConfig.dacGpio = DAC_CHANNEL_2_GPIO_NUM;
            fanConfig.dacChannel = DAC_CHANNEL_2;
            break;
        default:
            mjs_set_errorf(mjs, MJS_INTERNAL_ERROR, "%s: GPIO DAC param incorrect", TAG_FAN);
            mjs_return(mjs, MJS_INTERNAL_ERROR);
            return MJS_UNDEFINED;
    }
    if (ESP_OK == dac_output_enable(fanConfig.dacChannel)){
        mjs_set_errorf(mjs, MJS_INTERNAL_ERROR, "%s: Error to enable DAC", TAG_FAN);
        mjs_return(mjs, MJS_INTERNAL_ERROR);
        return MJS_UNDEFINED;
    }

    // ADC Init
    switch ( (gpio_num_t)gpioAdcj->valueint ) {
        case ADC1_CHANNEL_0_GPIO_NUM: // 36
            fanConfig.adcChannel = ADC1_CHANNEL_0;
            fanConfig.adcGpio = ADC1_CHANNEL_0_GPIO_NUM;
            break;
        case ADC1_CHANNEL_1_GPIO_NUM: // 37
            fanConfig.adcChannel = ADC1_CHANNEL_1;
            fanConfig.adcGpio = ADC1_CHANNEL_1_GPIO_NUM;
            break;
        case ADC1_CHANNEL_2_GPIO_NUM: // 38
            fanConfig.adcChannel = ADC1_CHANNEL_2;
            fanConfig.adcGpio = ADC1_CHANNEL_2_GPIO_NUM;
            break;
        case ADC1_CHANNEL_3_GPIO_NUM: // 39
            fanConfig.adcChannel = ADC1_CHANNEL_3;
            fanConfig.adcGpio = ADC1_CHANNEL_3_GPIO_NUM;
            break;
        case ADC1_CHANNEL_4_GPIO_NUM: // 32
            fanConfig.adcChannel = ADC1_CHANNEL_4;
            fanConfig.adcGpio = ADC1_CHANNEL_4_GPIO_NUM;
            break;
        case ADC1_CHANNEL_5_GPIO_NUM: // 33
            fanConfig.adcChannel = ADC1_CHANNEL_5;
            fanConfig.adcGpio = ADC1_CHANNEL_5_GPIO_NUM;
            break;
        case ADC1_CHANNEL_6_GPIO_NUM: // 34
            fanConfig.adcChannel = ADC1_CHANNEL_6;
            fanConfig.adcGpio = ADC1_CHANNEL_6_GPIO_NUM;
            break;
        case ADC1_CHANNEL_7_GPIO_NUM: // 35
            fanConfig.adcChannel = ADC1_CHANNEL_7;
            fanConfig.adcGpio = ADC1_CHANNEL_7_GPIO_NUM;
            break;
        default:
            mjs_set_errorf(mjs, MJS_INTERNAL_ERROR, "%s: GPIO ADC param incorrect", TAG_FAN);
            mjs_return(mjs, MJS_INTERNAL_ERROR);
            return MJS_UNDEFINED;
    };

    adc_power_on();

    if ( ESP_OK != adc_gpio_init( ADC_UNIT, fanConfig.adcGpio ) ){
        mjs_set_errorf(mjs, MJS_INTERNAL_ERROR, "%s: GPIO ADC init error", TAG_FAN);
        mjs_return(mjs, MJS_INTERNAL_ERROR);
        return MJS_UNDEFINED;
    }

    adc1_config_width( ADC_WIDTH );

    adc1_config_channel_atten( fanConfig.adcChannel, ADC_ATTEN );

    // PCNT Init
    if ( ESP_OK != pcnt_init ((gpio_num_t)gpioCntj->valueint) ){
        mjs_set_errorf(mjs, MJS_INTERNAL_ERROR, "%s: PCNT init error", TAG_FAN);
        mjs_return(mjs, MJS_INTERNAL_ERROR);
        return MJS_UNDEFINED;
    }

    //Add protected property to interface
    //stdi_setProtectedProperty(mjs, interface, "dacGpio", mjs_mk_number(mjs, gpio));

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

void thingjsFanRegister(void) {
    static int thingjs_Fan_cases[] = DEF_CASES(
            DEF_CASE(
                    DEF_ENUM( DEF_CASE(GPIO25), DEF_CASE(GPIO26) ), // DAC GPIO
                    DEF_ENUM( DEF_CASE(GPIO32), DEF_CASE(GPIO33), DEF_CASE(GPIO34), // ADC GPIO
                              DEF_CASE(GPIO35), DEF_CASE(GPIO36), DEF_CASE(GPIO39)),
                    DEF_ENUM( DEF_CASE(GPIO0),  DEF_CASE(GPIO2), DEF_CASE(GPIO3), DEF_CASE(GPIO4), // PCNT GPIO
                              DEF_CASE(GPIO5),  DEF_CASE(GPIO12), DEF_CASE(GPIO13), DEF_CASE(GPIO14),
                              DEF_CASE(GPIO15), DEF_CASE(GPIO16), DEF_CASE(GPIO17), DEF_CASE(GPIO18),
                              DEF_CASE(GPIO19), DEF_CASE(GPIO21), DEF_CASE(GPIO22), DEF_CASE(GPIO23),
                              DEF_CASE(GPIO25), DEF_CASE(GPIO26), DEF_CASE(GPIO27), DEF_CASE(GPIO32),
                              DEF_CASE(GPIO33), DEF_CASE(GPIO34), DEF_CASE(GPIO35), DEF_CASE(GPIO36),
                              DEF_CASE(GPIO39)
                            )
            )
    );

    static const struct st_thingjs_interface_manifest interface = {
            .type           = "fan",
            .constructor    = thingjsFanConstructor,
            .destructor     = NULL,
            .cases          = thingjs_dac_cases
    };

    thingjsRegisterInterface(&interface);
}



