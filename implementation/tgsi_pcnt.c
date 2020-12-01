/*
 *  Created on: 26 nov. 2020 г.
 *      Author: nazguluz
 */

#include "tgsi_pcnt.h"
#include "driver/pcnt.h"
#include <esp_log.h>
#include "sdti_utils.h"
#include "thingjs_board.h"
#include "thingjs_core.h"

#define PCNT_UNIT PCNT_UNIT_0
#define PCNT_CHANNEL PCNT_CHANNEL_0
#define PCNT_H_LIM_VAL 32767
#define PCNT_L_LIM_VAL -10
#define PCNT_FILTER_VALUE 10

const char TAG_PCNT[] = "PCNT";

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

gpio_num_t getPcntGPIO(struct mjs *mjs) {
    //Get this object that store params
    mjs_val_t this_obj = mjs_get_this(mjs);
    //Get internal params
    return (gpio_num_t) mjs_get_int32(mjs, mjs_get(mjs, this_obj, "pcnt", ~0));
}

static void thingjsPcntGetCount (struct mjs *mjs) {
    int16_t count = 0;
    pcnt_get_counter_value(PCNT_UNIT, &count);
    mjs_return(mjs, mjs_mk_number(mjs, count));
}

static void thingjsPcntResetCounter (struct mjs *mjs) {
    pcnt_counter_clear(PCNT_UNIT);
    mjs_return(mjs, MJS_UNDEFINED);
}

mjs_val_t thingjsPcntConstructor(struct mjs *mjs, cJSON *params) {
    //Validate preset params
    if (!cJSON_IsArray(params) || !(cJSON_GetArraySize(params) == 2)) {
        mjs_set_errorf(mjs, MJS_INTERNAL_ERROR, "%s: Incorrect params", TAG_PCNT);
        mjs_return(mjs, MJS_INTERNAL_ERROR);
        return MJS_UNDEFINED;
    }

    cJSON * unit = cJSON_GetArrayItem(params, 0);
    cJSON * gpio = cJSON_GetArrayItem(params, 1);

    if (!cJSON_IsNumber(unit) || !cJSON_IsNumber(gpio)) {
        mjs_set_errorf(mjs, MJS_INTERNAL_ERROR, "%s: Incorrect params", TAG_PCNT);
        mjs_return(mjs, MJS_INTERNAL_ERROR);
        return MJS_UNDEFINED;
    }

    //Create mjs object
    mjs_val_t interface = mjs_mk_object(mjs);

    if ( ESP_OK != pcnt_init( (pcnt_unit_t)unit->valueint , (gpio_num_t)gpio->valueint ) ) {
        mjs_set_errorf(mjs, MJS_INTERNAL_ERROR, "%s: Error to initialise", TAG_PCNT);
        mjs_return(mjs, MJS_INTERNAL_ERROR);
        return MJS_UNDEFINED;
    }

    //Add protected property to interface
    stdi_setProtectedProperty(mjs, interface, "pcnt", mjs_mk_number(mjs, gpio->valueint));

    //Bind functions
    stdi_setProtectedProperty(mjs, interface, "getCount",
                              mjs_mk_foreign_func(mjs, (mjs_func_ptr_t) thingjsPcntGetCount));
    stdi_setProtectedProperty(mjs, interface, "resetCounter",
                              mjs_mk_foreign_func(mjs, (mjs_func_ptr_t) thingjsPcntResetCounter));

    //Return mJS interface object
    return interface;
}

void thingjsPcntRegister(void) {
    static int thingjs_pcnt_cases[] = DEF_CASES(
            DEF_CASE(
                    DEF_ENUM(RES_PCNT_0, RES_PCNT_1, RES_PCNT_2, RES_PCNT_3, RES_PCNT_4, RES_PCNT_5, RES_PCNT_6, RES_PCNT_7),
                    DEF_ENUM(
                            GPIO2, GPIO4, GPIO5, GPIO13, GPIO14, GPIO15, GPIO16, GPIO17, GPIO18, GPIO19, GPIO21, GPIO22,
                            GPIO23, GPIO25, GPIO26, GPIO27, GPIO32, GPIO33, GPIO34, GPIO35, GPIO36, GPIO39
                    )
            )
    );

    static const struct st_thingjs_interface_manifest interface = {
            .type           = "pcnt",
            .constructor    = thingjsPcntConstructor,
            .destructor     = NULL,
            .cases          = thingjs_pcnt_cases
    };

    thingjsRegisterInterface(&interface);
}



