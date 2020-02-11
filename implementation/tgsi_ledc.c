/*
 *  Created on: 21 мар. 2019 г.
 *      Author: rpiontik
 */

#include <string.h>

#include "tgsi_ledc.h"

#include "driver/ledc.h"
#include <esp_log.h>

#include "sdti_utils.h"
#include "thingjs_board.h"
#include "thingjs_core.h"

const char TAG_LEDC[] = "LEDC";

#define  INTERFACE_NAME "ledc"

const char DEF_STR_FREQUENCY[]      = "frequency";
const char DEF_STR_RESOLUTION[]     = "resolution";
const char DEF_STR_SPEED_MODE[]     = "speed_mode";
const char DEF_STR_TIMER[]          = "timer";
const char DEF_STR_CHANNEL[]        = "channel";
const char DEF_STR_CHANNELS[]       = "channels";
const char DEF_STR_DRIVER[]         = "driver";
const char DEF_STR_INVERSE[]        = "inverse";
const char DEF_STR_DUTY[]           = "duty";
const char DEF_STR_GPIO[]           = "gpio";
const char DEF_STR_RECONFIG[]       = "reconfig";
const char DEF_STR_FADE[]           = "fade";

#define LEDC_CONFIG_DEFAULT_FREQUENCY   2440
#define LEDC_CONFIG_DEFAULT_RESOLUTION  15

static void thingjsLEDCSetFadeTimeAndStart(struct mjs *mjs) {
    ESP_LOGD(TAG_LEDC, "START FADE");

    //Get function params
    mjs_val_t arg0 = mjs_arg(mjs, 0);   //Target duty
    mjs_val_t arg1 = mjs_arg(mjs, 1);   //Fade ms

    //Validate params
    if(!mjs_is_number(arg0) || !mjs_is_number(arg1)) {
        mjs_set_errorf(mjs, MJS_INTERNAL_ERROR, "%s: Error params of function setFadeTimeAndStart(int, int)", TAG_LEDC);
        mjs_return(mjs, MJS_INTERNAL_ERROR);
        return;
    }

    //Get this mjs object (context)
    const mjs_val_t this_obj = mjs_get_this(mjs);
    //Get current the channel
    const int channel = mjs_get_int32(mjs, mjs_get(mjs, this_obj, DEF_STR_CHANNEL, ~0));
    //Get driver of the channel
    const mjs_val_t mjs_driver = mjs_get(mjs, this_obj, DEF_STR_DRIVER, ~0);
    //Get speed mode of the driver
    const int speed_mode = mjs_get_int32(mjs, mjs_get(mjs, mjs_driver, DEF_STR_SPEED_MODE, ~0));

    const uint32_t target_duty = (uint32_t)mjs_get_int32(mjs, arg0) & 0xFFFF;
    const unsigned long fade_ms = mjs_get_int32(mjs, arg1);

    esp_err_t result = ledc_set_fade_time_and_start(speed_mode, channel, target_duty, fade_ms, LEDC_FADE_NO_WAIT);

    if (ESP_OK != result) {
        mjs_set_errorf(mjs, MJS_INTERNAL_ERROR, "%s: Can not start fade function channel=[%d] speed_mode=[%d] target_duty=[%d] fade_ms=[%lu]",
                TAG_LEDC, channel, speed_mode, target_duty, fade_ms);
        mjs_return(mjs, MJS_INTERNAL_ERROR);
        return;
    }

    mjs_return(mjs, MJS_OK);
    ESP_LOGD(TAG_LEDC, "END FADE");
}

static void thingjsLEDCReconfigChannel(struct mjs *mjs) {
    ESP_LOGD(TAG_LEDC, "START RECONFIG CHANNEL");
    //Get this mjs object (context)
    const mjs_val_t this_obj = mjs_get_this(mjs);
    //Get inverse mode
    bool inverse = mjs_get_bool(mjs, mjs_get(mjs, this_obj, DEF_STR_INVERSE,  ~0));
    //Get duty value
    uint32_t duty = mjs_get_int32(mjs, mjs_get(mjs, this_obj, DEF_STR_DUTY,  ~0));

    //Check found params
    const mjs_val_t arg0 = mjs_arg(mjs, 0);   //Target time
    if (mjs_is_object(arg0)){
        //Get inverse flag (if found)
        const mjs_val_t mjs_inverse = mjs_get(mjs, arg0, DEF_STR_INVERSE, ~0);
        if(mjs_is_boolean(mjs_inverse))
            inverse = mjs_get_bool(mjs, mjs_inverse);

        //Get duty param (if found)
        const mjs_val_t mjs_duty = mjs_get(mjs, arg0, DEF_STR_DUTY, ~0);
        if(mjs_is_number(mjs_inverse))
            duty = mjs_get_int32(mjs, mjs_duty);
    }

    ledc_channel_config_t ledc_channel  = {0};

    //Get driver/timer
    const mjs_val_t mjs_driver = mjs_get(mjs, this_obj, DEF_STR_DRIVER, ~0);
    const mjs_val_t mjs_timer = mjs_get(mjs, mjs_driver, DEF_STR_TIMER, ~0);
    const mjs_val_t resolution = mjs_get_int32(mjs, mjs_get(mjs, mjs_driver, DEF_STR_RESOLUTION, ~0));
    const int timer = mjs_get_int32(mjs, mjs_timer);

    switch(timer) {
        case RES_LEDC_0:
            ledc_channel.timer_sel  = LEDC_TIMER_0;
            break;
        case RES_LEDC_1:
            ledc_channel.timer_sel  = LEDC_TIMER_1;
            break;
        default:
            mjs_set_errorf(mjs, MJS_INTERNAL_ERROR, "%s: Incorrect reconfig() params", TAG_LEDC);
            mjs_return(mjs, MJS_INTERNAL_ERROR);
            return;
    }

    //Get channel
    ledc_channel.channel    = mjs_get_int32(mjs, mjs_get(mjs, this_obj, DEF_STR_CHANNEL, ~0));
    ledc_channel.gpio_num   = mjs_get_int32(mjs, mjs_get(mjs, this_obj, DEF_STR_GPIO, ~0));
    ledc_channel.speed_mode = LEDC_HIGH_SPEED_MODE;
    ledc_channel.duty       = inverse ? (1 << resolution) - 1 : 0;

    ESP_LOGD(TAG_LEDC, "BEFORE RECONFIG CHANNEL");
    esp_err_t result = ledc_channel_config(&ledc_channel);
    ESP_LOGD(TAG_LEDC, "AFTER RECONFIG CHANNEL");

    if(ESP_OK == result) {
        stdi_setProtectedProperty(mjs, this_obj, DEF_STR_INVERSE, mjs_mk_boolean(mjs, inverse));
        stdi_setProtectedProperty(mjs, this_obj, DEF_STR_DUTY, mjs_mk_number(mjs, duty));
    } else {
        mjs_set_errorf(mjs, MJS_INTERNAL_ERROR, "%s: Can not apply reconfig() params", TAG_LEDC);
        mjs_return(mjs, MJS_INTERNAL_ERROR);
    }

    mjs_return(mjs, MJS_OK);
    ESP_LOGD(TAG_LEDC, "END RECONFIG CHANNEL");
}

static void thingjsLEDCReconfigDriver(struct mjs *mjs) {
    ESP_LOGD(TAG_LEDC, "START RECONFIG DRIVER");
    //Get this mjs object (context)
    const mjs_val_t this_obj = mjs_get_this(mjs);

    //Get current frequency
    uint32_t frequency = mjs_get_int32(mjs, mjs_get(mjs, this_obj, DEF_STR_FREQUENCY, ~0));
    uint32_t resolution = mjs_get_int32(mjs, mjs_get(mjs, this_obj, DEF_STR_RESOLUTION, ~0));
    uint32_t speed_mode = mjs_get_int32(mjs, mjs_get(mjs, this_obj, DEF_STR_SPEED_MODE, ~0));
    uint32_t timer = mjs_get_int32(mjs, mjs_get(mjs, this_obj, DEF_STR_TIMER, ~0));

    //Get function params
    const mjs_val_t arg0 = mjs_arg(mjs, 0);   //Params
    if(mjs_is_object(arg0)) {
        //Get frequency (if found)
        const mjs_val_t mjs_frequency = mjs_get(mjs, arg0, DEF_STR_FREQUENCY, ~0);
        if(mjs_is_number(mjs_frequency))
            frequency = mjs_get_int32(mjs, mjs_frequency);

        //Get resolution (if found)
        const mjs_val_t mjs_resolution = mjs_get(mjs, arg0, DEF_STR_RESOLUTION, ~0);
        if(mjs_is_number(mjs_resolution))
            resolution = mjs_get_int32(mjs, mjs_resolution);
    }

    ledc_timer_config_t ledc_timer = {
            .duty_resolution    = resolution, 			// resolution of PWM duty
            .freq_hz            = frequency,            // frequency of PWM signal
            .speed_mode         = speed_mode 	        // timer mode
    };

    //Set timer index
    switch(timer) {
        case RES_LEDC_0:
            ledc_timer.timer_num = LEDC_TIMER_0;
            break;
        case RES_LEDC_1:
            ledc_timer.timer_num = LEDC_TIMER_1;
            break;
    }

    // Set configuration of timer0 for high speed channels
    esp_err_t result = ledc_timer_config(&ledc_timer);
    if ( ESP_OK == result ) {
        stdi_setProtectedProperty(mjs, this_obj, DEF_STR_FREQUENCY, mjs_mk_boolean(mjs, frequency));
        stdi_setProtectedProperty(mjs, this_obj, DEF_STR_RESOLUTION, mjs_mk_number(mjs, resolution));
    } else {
        mjs_set_errorf(mjs, MJS_INTERNAL_ERROR, "%s: Can not apply config of timer[%d] resolution=[%d] freq_hz=[%d]",
                       TAG_LEDC, timer, resolution, frequency);
        mjs_return(mjs, MJS_INTERNAL_ERROR);
        return;
    }

    mjs_return(mjs, MJS_OK);
    ESP_LOGD(TAG_LEDC, "END RECONFIG DRIVER");
}

mjs_val_t thingjsLEDCConstructor(struct mjs *mjs, cJSON *params) {
    //Validate preset params
    if(!cJSON_IsArray(params) || (cJSON_GetArraySize(params) < 2)) {
        mjs_set_errorf(mjs, MJS_INTERNAL_ERROR, "%s: Error params", TAG_LEDC);
        return MJS_INTERNAL_ERROR;
    }

    //Create mJS interface object
    mjs_val_t interface = mjs_mk_object(mjs);

    //Add protected property to interface
    stdi_setProtectedProperty(mjs, interface, DEF_STR_SPEED_MODE, mjs_mk_number(mjs, LEDC_HIGH_SPEED_MODE));
    stdi_setProtectedProperty(mjs, interface, DEF_STR_TIMER, mjs_mk_number(mjs, cJSON_GetArrayItem(params, 0)->valueint));
    stdi_setProtectedProperty(mjs, interface, DEF_STR_FREQUENCY, mjs_mk_number(mjs, LEDC_CONFIG_DEFAULT_FREQUENCY));
    stdi_setProtectedProperty(mjs, interface, DEF_STR_RESOLUTION, mjs_mk_number(mjs, LEDC_CONFIG_DEFAULT_RESOLUTION));
    stdi_setProtectedProperty(mjs, interface, DEF_STR_RECONFIG,
            mjs_mk_foreign_func(mjs, (mjs_func_ptr_t) thingjsLEDCReconfigDriver));

    //Make the channels array
    mjs_val_t channels = mjs_mk_array(mjs);
    for(int i = 1; i < cJSON_GetArraySize(params); i++) {
        //Create channel object
        mjs_val_t channel = mjs_mk_object(mjs);
        //Get GPIO of channel
        const cJSON * json_gpio = cJSON_GetArrayItem(params, i);
        if(!cJSON_IsNumber(json_gpio)) {
            mjs_set_errorf(mjs, MJS_INTERNAL_ERROR, "%s: Error params", TAG_LEDC);
            return MJS_INTERNAL_ERROR;
        }
        //Bind properties
        stdi_setProtectedProperty(mjs, channel, DEF_STR_DRIVER, interface);
        stdi_setProtectedProperty(mjs, channel, DEF_STR_CHANNEL, mjs_mk_number(mjs,  i - 1));
        stdi_setProtectedProperty(mjs, channel, DEF_STR_GPIO, mjs_mk_number(mjs,  json_gpio->valueint));
        stdi_setProtectedProperty(mjs, channel, DEF_STR_DUTY, mjs_mk_number(mjs, 0));
        stdi_setProtectedProperty(mjs, channel, DEF_STR_INVERSE, mjs_mk_boolean(mjs, 1));
        //Bind functions
        stdi_setProtectedProperty(mjs, channel, DEF_STR_RECONFIG,
                mjs_mk_foreign_func(mjs, (mjs_func_ptr_t) thingjsLEDCReconfigChannel));
        stdi_setProtectedProperty(mjs, channel, DEF_STR_FADE,
                mjs_mk_foreign_func(mjs, (mjs_func_ptr_t) thingjsLEDCSetFadeTimeAndStart));
        mjs_array_push(mjs, channels, channel);
    }
    //Append the channels array
    stdi_setProtectedProperty(mjs, interface, DEF_STR_CHANNELS, channels);

    //Return mJS interface object
    return interface;
}

void thingjsLEDCRegister(void) {
    ledc_fade_func_install(0);

    static int thingjs_ledc_cases[] = DEF_CASES(
            DEF_CASE(
                    DEF_ENUM(RES_LEDC_0, RES_LEDC_1),
                    DEF_ENUM(
                            GPIO0, GPIO1, GPIO2, GPIO3, GPIO4, GPIO5, GPIO12, GPIO13, GPIO14, GPIO15, GPIO16, GPIO17, GPIO18,
                            GPIO19, GPIO21, GPIO22, GPIO23, GPIO25, GPIO26, GPIO27, GPIO32, GPIO33
                            )
                    ,DEF_ENUM(
                            GPIO0, GPIO1, GPIO2, GPIO3, GPIO4, GPIO5, GPIO12, GPIO13, GPIO14, GPIO15, GPIO16, GPIO17, GPIO18,
                            GPIO19, GPIO21, GPIO22, GPIO23, GPIO25, GPIO26, GPIO27, GPIO32, GPIO33
                    )
                    ,DEF_ENUM(
                            GPIO0, GPIO1, GPIO2, GPIO3, GPIO4, GPIO5, GPIO12, GPIO13, GPIO14, GPIO15, GPIO16, GPIO17, GPIO18,
                            GPIO19, GPIO21, GPIO22, GPIO23, GPIO25, GPIO26, GPIO27, GPIO32, GPIO33
                    )
                    ,DEF_ENUM(
                            GPIO0, GPIO1, GPIO2, GPIO3, GPIO4, GPIO5, GPIO12, GPIO13, GPIO14, GPIO15, GPIO16, GPIO17, GPIO18,
                            GPIO19, GPIO21, GPIO22, GPIO23, GPIO25, GPIO26, GPIO27, GPIO32, GPIO33
                    )
                    ,DEF_ENUM(
                            GPIO0, GPIO1, GPIO2, GPIO3, GPIO4, GPIO5, GPIO12, GPIO13, GPIO14, GPIO15, GPIO16, GPIO17, GPIO18,
                            GPIO19, GPIO21, GPIO22, GPIO23, GPIO25, GPIO26, GPIO27, GPIO32, GPIO33
                    )
                    ,DEF_ENUM(
                            GPIO0, GPIO1, GPIO2, GPIO3, GPIO4, GPIO5, GPIO12, GPIO13, GPIO14, GPIO15, GPIO16, GPIO17, GPIO18,
                            GPIO19, GPIO21, GPIO22, GPIO23, GPIO25, GPIO26, GPIO27, GPIO32, GPIO33
                    )
                    ,DEF_ENUM(
                            GPIO0, GPIO1, GPIO2, GPIO3, GPIO4, GPIO5, GPIO12, GPIO13, GPIO14, GPIO15, GPIO16, GPIO17, GPIO18,
                            GPIO19, GPIO21, GPIO22, GPIO23, GPIO25, GPIO26, GPIO27, GPIO32, GPIO33
                    )
                    ,DEF_ENUM(
                            GPIO0, GPIO1, GPIO2, GPIO3, GPIO4, GPIO5, GPIO12, GPIO13, GPIO14, GPIO15, GPIO16, GPIO17, GPIO18,
                            GPIO19, GPIO21, GPIO22, GPIO23, GPIO25, GPIO26, GPIO27, GPIO32, GPIO33
                    )
            )
    );

    static const struct st_thingjs_interface_manifest interface = {
            .type           = INTERFACE_NAME,
            .constructor    = thingjsLEDCConstructor,
            .destructor     = NULL,
            .cases          = thingjs_ledc_cases
    };

    thingjsRegisterInterface(&interface);
}



