#include <dirent.h>
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
#include <freertos/FreeRTOS.h>
#include "freertos/task.h"

const char TAG_FAN[] = "FAN";

#define FAN_DEMON_DELAY_MS 1000

#define ADC_UNIT  ADC_UNIT_1
#define ADC_WIDTH ADC_WIDTH_BIT_12
#define ADC_ATTEN ADC_ATTEN_DB_11

#define PCNT_UNIT PCNT_UNIT_0
#define PCNT_CHANNEL PCNT_CHANNEL_0
#define PCNT_H_LIM_VAL 32767
#define PCNT_L_LIM_VAL -10
#define PCNT_FILTER_VALUE 5

static TaskHandle_t fan_demon_handle = 0;

typedef struct {
    dac_channel_t   dacChannel;
    gpio_num_t      dacGpio;
    adc1_channel_t  adcChannel;
    gpio_num_t      adcGpio;
    gpio_num_t      pcntGpio;
} fan_config_t;

static fan_config_t fanConfig = {
    .dacChannel = DAC_CHANNEL_MAX,
    .dacGpio    = GPIO_NUM_NC,
    .adcChannel = ADC1_CHANNEL_MAX,
    .adcGpio    = GPIO_NUM_NC,
    .pcntGpio   = GPIO_NUM_NC
};

typedef struct {
    int32_t rpm;
    int32_t adcRaw;
    uint8_t dac;
    float value;
    float tempCurrent;
} fan_state_t;

static fan_state_t fanState = {
        .dac = 0,
        .rpm = -1,
        .tempCurrent = -1,
        .value = 0,
        .adcRaw = -1
};

/* Initialize PCNT functions:
 *  - configure and initialize PCNT
 *  - set up the input filter
 *  - set up the counter events to watch
 */
static esp_err_t pcnt_init(pcnt_unit_t unit, gpio_num_t gpio) {
    /* Prepare configuration for the PCNT unit */
    pcnt_config_t pcnt_config = {
            // Set PCNT input signal and control GPIOs
            .pulse_gpio_num = gpio,
            .ctrl_gpio_num = -1,
            .channel = PCNT_CHANNEL,
            .unit = unit,
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

static void thingjsFanReconfig(struct mjs *mjs) {
    mjs_return(mjs, MJS_UNDEFINED);
}
static void thingjsFanSetVal(struct mjs *mjs) {
    //Get function params
    mjs_val_t arg0 = mjs_arg(mjs, 0);
    //Param validation
    if (mjs_is_number(arg0)) {
        /* Set the DAC voltage */
        uint32_t val = mjs_get_int32(mjs, arg0);
        if (val > 255) val = 255;
        if (ESP_OK != dac_output_voltage(fanConfig.dacChannel, (uint8_t)val )){
            mjs_set_errorf(mjs, MJS_INTERNAL_ERROR, "%s: Error to set value in function SetVoltage", TAG_FAN);
            mjs_return(mjs, MJS_INTERNAL_ERROR);
            return;
        }
        fanState.dac = val;
    } else {
        mjs_set_errorf(mjs, MJS_INTERNAL_ERROR, "%s: Incorrect call function SetVoltage", TAG_FAN);
        mjs_return(mjs, MJS_INTERNAL_ERROR);
        return;
    }
    mjs_return(mjs, MJS_UNDEFINED);
}
static void thingjsFanGetRpm(struct mjs *mjs) {
    mjs_return(mjs, mjs_mk_number( mjs, fanState.rpm ));
}
static void thingjsFanGetVolt(struct mjs *mjs) {
    mjs_return(mjs, mjs_mk_number( mjs, fanState.adcRaw ));
}

//Background demon of FAN controller
_Noreturn static void thingjsFanDaemon(void *data) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    while(1) {
        vTaskDelayUntil( &xLastWakeTime, FAN_DEMON_DELAY_MS);
        int16_t count = 0;
        pcnt_get_counter_value(PCNT_UNIT, &count);
        pcnt_counter_clear(PCNT_UNIT);
        if (count > 0) {
            fanState.rpm = count * 60;
        } else fanState.rpm = 0;
        dac_output_voltage(fanConfig.dacChannel,fanState.dac);
        fanState.adcRaw = adc1_get_raw(fanConfig.adcChannel);
    }
}

mjs_val_t thingjsFanConstructor(struct mjs *mjs, cJSON *params) {
    //Validate preset params
    if (!cJSON_IsArray(params) || !(cJSON_GetArraySize(params) == 4)) {
        mjs_set_errorf(mjs, MJS_INTERNAL_ERROR, "%s: Incorrect params", TAG_FAN);
        mjs_return(mjs, MJS_INTERNAL_ERROR);
        return MJS_UNDEFINED;
    }

    cJSON * gpioDacj = cJSON_GetArrayItem(params, 0); // DAC Gpio
    cJSON * gpioAdcj = cJSON_GetArrayItem(params, 1); // ADC Gpio
    cJSON * pcntUnitj = cJSON_GetArrayItem(params, 2); // TAH/RPM Unit
    cJSON * pcntGpioj = cJSON_GetArrayItem(params, 3); // TAH/RPM Gpio

    if (!cJSON_IsNumber(gpioDacj) || !cJSON_IsNumber(gpioAdcj) || !cJSON_IsNumber(pcntUnitj) || !cJSON_IsNumber(pcntGpioj) ) {
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
        case GPIO25:
            fanConfig.dacGpio = GPIO25;
            fanConfig.dacChannel = DAC_CHANNEL_1;
            break;
        case GPIO26:
            fanConfig.dacGpio = GPIO26;
            fanConfig.dacChannel = DAC_CHANNEL_2;
            break;
        default:
            mjs_set_errorf(mjs, MJS_INTERNAL_ERROR, "%s: GPIO DAC param incorrect", TAG_FAN);
            mjs_return(mjs, MJS_INTERNAL_ERROR);
            return MJS_UNDEFINED;
    }
    if (ESP_OK != dac_output_enable(fanConfig.dacChannel)){
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

    if ( ESP_OK != adc_gpio_init( ADC_UNIT, fanConfig.adcChannel ) ){
        mjs_set_errorf(mjs, MJS_INTERNAL_ERROR, "%s: GPIO ADC init error", TAG_FAN);
        mjs_return(mjs, MJS_INTERNAL_ERROR);
        return MJS_UNDEFINED;
    }

    adc1_config_width( ADC_WIDTH );

    adc1_config_channel_atten( fanConfig.adcChannel, ADC_ATTEN );

    // PCNT Init
    pcnt_unit_t u;
    switch (pcntUnitj->valueint) {
        case RES_PCNT_0:
            u = PCNT_UNIT_0;
            break;
        case RES_PCNT_1:
            u = PCNT_UNIT_1;
            break;
        case RES_PCNT_2:
            u = PCNT_UNIT_2;
            break;
        case RES_PCNT_3:
            u = PCNT_UNIT_3;
            break;
        case RES_PCNT_4:
            u = PCNT_UNIT_4;
            break;
        case RES_PCNT_5:
            u = PCNT_UNIT_5;
            break;
        case RES_PCNT_6:
            u = PCNT_UNIT_6;
            break;
        case RES_PCNT_7:
            u = PCNT_UNIT_7;
            break;
        default:
            mjs_set_errorf(mjs, MJS_INTERNAL_ERROR, "%s: PCNT unit param error", TAG_FAN);
            mjs_return(mjs, MJS_INTERNAL_ERROR);
            return MJS_UNDEFINED;
    }

    if ( ESP_OK != pcnt_init ( u ,(gpio_num_t)pcntGpioj->valueint) ){
        mjs_set_errorf(mjs, MJS_INTERNAL_ERROR, "%s: PCNT init error", TAG_FAN);
        mjs_return(mjs, MJS_INTERNAL_ERROR);
        return MJS_UNDEFINED;
    }

    //Add protected property to interface
    //stdi_setProtectedProperty(mjs, interface, "dacGpio", mjs_mk_number(mjs, gpio));

    //Bind functions
    stdi_setProtectedProperty(mjs, interface, "reconfig",
                              mjs_mk_foreign_func(mjs, (mjs_func_ptr_t) thingjsFanReconfig));
    stdi_setProtectedProperty(mjs, interface, "setVal",
                              mjs_mk_foreign_func(mjs, (mjs_func_ptr_t) thingjsFanSetVal));
    stdi_setProtectedProperty(mjs, interface, "getRpm",
                              mjs_mk_foreign_func(mjs, (mjs_func_ptr_t) thingjsFanGetRpm));
    stdi_setProtectedProperty(mjs, interface, "getVolt",
                              mjs_mk_foreign_func(mjs, (mjs_func_ptr_t) thingjsFanGetVolt));

    xTaskCreatePinnedToCore(
            &thingjsFanDaemon,
            "SmartLED",
            2000,
            NULL,
            5,
            &fan_demon_handle,
            0
    );

    //Return mJS interface object
    return interface;
}

void thingjsFanRegister(void) {
    static int thingjs_fan_cases[] = DEF_CASES(
            DEF_CASE(
                    DEF_ENUM( DEF_CASE(GPIO25), DEF_CASE(GPIO26) ), // DAC GPIO
                    DEF_ENUM( DEF_CASE(GPIO32), DEF_CASE(GPIO33), DEF_CASE(GPIO34), // ADC GPIO
                              DEF_CASE(GPIO35), DEF_CASE(GPIO36), DEF_CASE(GPIO39)),
                    DEF_ENUM(RES_PCNT_0, RES_PCNT_1, RES_PCNT_2, RES_PCNT_3, RES_PCNT_4, RES_PCNT_5, RES_PCNT_6, RES_PCNT_7), // PCNT UNIT
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
            .cases          = thingjs_fan_cases
    };

    thingjsRegisterInterface(&interface);
}



