/*
 *  Created on: 21 мар. 2019 г.
 *      Author: rpiontik
 */

#include <string.h>

#include "tgsi_smart_led.h"

#include <freertos/FreeRTOS.h>
#include "freertos/queue.h"
#include "driver/ledc.h"
#include <esp_log.h>
#include <freertos/queue.h>

#include "sdti_utils.h"
#include "thingjs_board.h"
#include "thingjs_core.h"

const char TAG_SMARTLED[] = "SmartLED";

#define  INTERFACE_NAME "SmartLED"

const char DEF_STR_FREQUENCY[]      = "frequency";
const char DEF_STR_RESOLUTION[]     = "resolution";
const char DEF_STR_CONTROLLER[]     = "controller";
const char DEF_STR_TIMER[]          = "timer";
const char DEF_STR_CHANNEL[]        = "channel";
const char DEF_STR_CHANNELS[]       = "channels";
const char DEF_STR_DRIVER[]         = "driver";
const char DEF_STR_INVERSE[]        = "inverse";
const char DEF_STR_DUTY[]           = "duty";
const char DEF_STR_GPIO[]           = "gpio";
const char DEF_STR_RECONFIG[]       = "reconfig";
const char DEF_STR_FADE[]           = "fade";

#define SMARTLED_CONFIG_DEFAULT_FREQUENCY   2440
#define SMARTLED_CONFIG_DEFAULT_RESOLUTION  15
#define SMARTLED_CONFIG_DEFAULT_TIMER       LEDC_TIMER_0

#define MAX_CHANNELS    8
#define MAX_CONTROLLER  2

#define FADE_DEMON_DELAY_MS    50

typedef enum {
    slac_go
} smartled_action;

struct st_smartled_action {
    smartled_action action;
    uint32_t controller;//Mode 0..(MAX_CONTROLLER - 1)
    uint32_t channel;   //Channel 0..(MAX_CHANNELS - 1)
    uint32_t target;    //Target duty
    uint32_t fade;      //Exposition
};

struct st_smartled_channel_state {
    uint32_t current_time;  //Current executiong time
    uint32_t fade;          //Time's exposition
    uint32_t duty_start;    //Start duty value
    uint32_t duty_target;   //Target duty value
};


//Count subscribers (processes) to SmartLED interface
static int smartLED_subscribers = 0;
static TaskHandle_t smartLED_demon_handle = 0;
static QueueHandle_t smartLED_demon_input = 0;

//Convert resource index controller to system index
inline int resControllerToSys(int controller){
    switch(controller) {
        case RES_LEDC_1:
            return LEDC_LOW_SPEED_MODE;
        case RES_LEDC_0:
        default:
            return LEDC_HIGH_SPEED_MODE;
    }
}

static uint32_t thingjsLEDCCalculateFadeValue(
        uint32_t dutyStart, uint32_t dutyStop, uint32_t fadeTimeMs, uint32_t fadeTimeCurrent) {

    if ((0 == fadeTimeMs) || (fadeTimeCurrent >= fadeTimeMs) || (dutyStart == dutyStop))
        return dutyStop;

    uint32_t dutyDiff = abs(dutyStart - dutyStop);
    uint32_t dutyCurrent = dutyStart + (dutyStart > dutyStop ? -1 : 1) *
            (uint32_t)((((((uint64_t) fadeTimeCurrent) << 10) * (uint64_t) dutyDiff) / (uint64_t) fadeTimeMs) >> 10);

    return dutyCurrent;
}

//Background demon of SmartLED
static void thingjsSmartLEDDemon(void *data) {
    static struct st_smartled_channel_state channels[MAX_CONTROLLER][MAX_CHANNELS] = {0};
    TickType_t xLastWakeTime = xTaskGetTickCount();
    while(1) {
        struct st_smartled_action q_message;
        if (xQueueReceive(smartLED_demon_input, &q_message, ( TickType_t ) 0)) {
            switch(q_message.action) {
                case slac_go:
                    if((q_message.controller) < MAX_CONTROLLER && (q_message.channel < MAX_CHANNELS)) {
                        struct st_smartled_channel_state *channel = &channels[q_message.controller][q_message.channel];
                        channel->current_time = 0;
                        channel->fade = q_message.fade;
                        channel->duty_target = q_message.target;
                        channel->duty_start = ledc_get_duty(q_message.controller, q_message.channel);
                    }
                    break;
            }
        }

        for (int controller_i = 0; controller_i < MAX_CONTROLLER; ++controller_i) {
            for (int channel_i = 0; channel_i < MAX_CHANNELS; ++channel_i) {
                struct st_smartled_channel_state *channel = &channels[controller_i][channel_i];

                if(channel->current_time >= channel->fade)
                    continue;

                uint32_t target_duty = thingjsLEDCCalculateFadeValue(
                            channel->duty_start,
                            channel->duty_target,
                            channel->fade,
                            channel->current_time
                        );

                if(target_duty != ledc_get_duty(controller_i, channel_i)) {
                    ledc_set_duty(controller_i, channel_i, target_duty);
                    ledc_update_duty(controller_i, channel_i);
                }

                channel->current_time += FADE_DEMON_DELAY_MS;
            }
        }

        vTaskDelayUntil( &xLastWakeTime, FADE_DEMON_DELAY_MS);
    }
}

static void thingjsSmartLEDSubscribe(void) {
    if(!smartLED_subscribers) {
        ledc_fade_func_install(0);
        xTaskCreatePinnedToCore(
                &thingjsSmartLEDDemon,
                "SmartLED",
                2000,
                NULL,
                5,
                &smartLED_demon_handle,
                0
        );
    }
    ++smartLED_subscribers;
}

/*
 * todo will uncomment when need it
static void thingjsSmartLEDUnsubscribe(void) {
    if(!smartLED_subscribers)
        return;

    if(smartLED_subscribers == 1) {
        vTaskDelete(smartLED_demon_handle);
        ledc_fade_func_uninstall(0);
    }

    --smartLED_subscribers;
}
 */

static void thingjsSmartLEDSetFadeTimeAndStart(struct mjs *mjs) {
    ESP_LOGD(TAG_SMARTLED, "START FADE");

    //Get function params
    mjs_val_t arg0 = mjs_arg(mjs, 0);   //Target duty
    mjs_val_t arg1 = mjs_arg(mjs, 1);   //Fade ms

    //Validate params
    if(!mjs_is_number(arg0) || !mjs_is_number(arg1)) {
        mjs_set_errorf(mjs, MJS_INTERNAL_ERROR,
                "%s: Error params of function setFadeTimeAndStart(int, int)", TAG_SMARTLED);
        mjs_return(mjs, MJS_INTERNAL_ERROR);
        return;
    }

    //Get this mjs object (context)
    const mjs_val_t this_obj = mjs_get_this(mjs);
    //Get driver of the channel
    const mjs_val_t mjs_driver = mjs_get(mjs, this_obj, DEF_STR_DRIVER, ~0);

    //Make request to fade daemon
    struct st_smartled_action q_message = {
            //Action for daemon
            .action = slac_go,
            //Get speed mode of the driver
            .controller = resControllerToSys(
                    mjs_get_int32(mjs, mjs_get(mjs, mjs_driver, DEF_STR_CONTROLLER, ~0))
                    ),
            //Get current the channel
            .channel = mjs_get_int32(mjs, mjs_get(mjs, this_obj, DEF_STR_CHANNEL, ~0)),
            //Exposition
            .fade = (uint32_t)mjs_get_int32(mjs, arg1),
            //Target duty
            .target = (uint32_t)mjs_get_int32(mjs, arg0) & 0xFFFF
    };

    ESP_LOGD(TAG_SMARTLED, "APPLY FADE controller=%d, channel=%d, target=%d, fade_ms=%d",
             q_message.controller, q_message.channel, q_message.target, q_message.fade);

    xQueueSend(smartLED_demon_input, &q_message, ( TickType_t ) (FADE_DEMON_DELAY_MS * 2));

    mjs_return(mjs, MJS_OK);
    ESP_LOGD(TAG_SMARTLED, "END FADE");
}

static void thingjsSmartLEDReconfigChannel(struct mjs *mjs) {
    ESP_LOGD(TAG_SMARTLED, "START RECONFIG CHANNEL");
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
    const mjs_val_t resolution = mjs_get_int32(mjs, mjs_get(mjs, mjs_driver, DEF_STR_RESOLUTION, ~0));
    //Get channel
    ledc_channel.speed_mode  = resControllerToSys(
                mjs_get_int32(mjs, mjs_get(mjs, mjs_driver, DEF_STR_CONTROLLER, ~0))
            );
    ledc_channel.channel    = mjs_get_int32(mjs, mjs_get(mjs, this_obj, DEF_STR_CHANNEL, ~0));
    ledc_channel.gpio_num   = mjs_get_int32(mjs, mjs_get(mjs, this_obj, DEF_STR_GPIO, ~0));
    ledc_channel.timer_sel  = mjs_get_int32(mjs, mjs_get(mjs, this_obj, DEF_STR_TIMER, ~0));
    ledc_channel.duty       = inverse ? (1 << resolution) - 1 : 0;

    ESP_LOGD(TAG_SMARTLED, "BEFORE RECONFIG CHANNEL");
    gpio_set_direction(ledc_channel.gpio_num, GPIO_MODE_OUTPUT);
    esp_err_t result = ledc_channel_config(&ledc_channel);
    ESP_LOGD(TAG_SMARTLED, "AFTER RECONFIG CHANNEL");

    if(ESP_OK == result) {
        stdi_setProtectedProperty(mjs, this_obj, DEF_STR_INVERSE, mjs_mk_boolean(mjs, inverse));
        stdi_setProtectedProperty(mjs, this_obj, DEF_STR_DUTY, mjs_mk_number(mjs, duty));
    } else {
        mjs_set_errorf(mjs, MJS_INTERNAL_ERROR, "%s: Can not apply reconfig() params", TAG_SMARTLED);
        mjs_return(mjs, MJS_INTERNAL_ERROR);
    }

    mjs_return(mjs, MJS_OK);
    ESP_LOGD(TAG_SMARTLED, "END RECONFIG CHANNEL");
}

static void thingjsSmartLEDReconfigDriver(struct mjs *mjs) {
    ESP_LOGD(TAG_SMARTLED, "START RECONFIG DRIVER");
    //Get this mjs object (context)
    const mjs_val_t this_obj = mjs_get_this(mjs);

    //Get current frequency
    uint32_t frequency = mjs_get_int32(mjs, mjs_get(mjs, this_obj, DEF_STR_FREQUENCY, ~0));
    uint32_t resolution = mjs_get_int32(mjs, mjs_get(mjs, this_obj, DEF_STR_RESOLUTION, ~0));
    uint32_t controller = mjs_get_int32(mjs, mjs_get(mjs, this_obj, DEF_STR_CONTROLLER, ~0));
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
            .duty_resolution    = resolution, 			            // resolution of PWM duty
            .freq_hz            = frequency,                        // frequency of PWM signal
            .timer_num          = timer, 	                        // timer
            .speed_mode         = resControllerToSys(controller)    // Controller
    };

    // Set configuration of timer0 for high speed channels
    esp_err_t result = ledc_timer_config(&ledc_timer);
    if ( ESP_OK == result ) {
        stdi_setProtectedProperty(mjs, this_obj, DEF_STR_FREQUENCY, mjs_mk_boolean(mjs, frequency));
        stdi_setProtectedProperty(mjs, this_obj, DEF_STR_RESOLUTION, mjs_mk_number(mjs, resolution));
    } else {
        mjs_set_errorf(mjs, MJS_INTERNAL_ERROR,
                "%s: Can not apply config of timer[%d] resolution=[%d] freq_hz=[%d]",
                       TAG_SMARTLED, timer, resolution, frequency);
        mjs_return(mjs, MJS_INTERNAL_ERROR);
        return;
    }

    mjs_return(mjs, MJS_OK);
    ESP_LOGD(TAG_SMARTLED, "END RECONFIG DRIVER");
}

mjs_val_t thingjsSmartLEDConstructor(struct mjs *mjs, cJSON *params) {
    //Validate preset params
    if(!cJSON_IsArray(params) || (cJSON_GetArraySize(params) < 2)) {
        mjs_set_errorf(mjs, MJS_INTERNAL_ERROR, "%s: Error params", TAG_SMARTLED);
        return MJS_INTERNAL_ERROR;
    }

    //Subscribe on fade daemon
    thingjsSmartLEDSubscribe();

    //Create mJS interface object
    mjs_val_t interface = mjs_mk_object(mjs);

    //Add protected property to interface
    stdi_setProtectedProperty(mjs, interface, DEF_STR_CONTROLLER,
            mjs_mk_number(mjs, cJSON_GetArrayItem(params, 0)->valueint));
    stdi_setProtectedProperty(mjs, interface, DEF_STR_TIMER,
            mjs_mk_number(mjs, SMARTLED_CONFIG_DEFAULT_TIMER));
    stdi_setProtectedProperty(mjs, interface, DEF_STR_FREQUENCY,
            mjs_mk_number(mjs, SMARTLED_CONFIG_DEFAULT_FREQUENCY));
    stdi_setProtectedProperty(mjs, interface, DEF_STR_RESOLUTION,
            mjs_mk_number(mjs, SMARTLED_CONFIG_DEFAULT_RESOLUTION));
    stdi_setProtectedProperty(mjs, interface, DEF_STR_RECONFIG,
            mjs_mk_foreign_func(mjs, (mjs_func_ptr_t) thingjsSmartLEDReconfigDriver));

    //Make the channels array
    mjs_val_t channels = mjs_mk_array(mjs);
    for(int i = 1; i < cJSON_GetArraySize(params) && i <= MAX_CHANNELS; i++) {
        //Create channel object
        mjs_val_t channel = mjs_mk_object(mjs);
        //Get GPIO of channel
        const cJSON * json_gpio = cJSON_GetArrayItem(params, i);
        if(!cJSON_IsNumber(json_gpio)) {
            mjs_set_errorf(mjs, MJS_INTERNAL_ERROR, "%s: Error params", TAG_SMARTLED);
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
                mjs_mk_foreign_func(mjs, (mjs_func_ptr_t) thingjsSmartLEDReconfigChannel));
        stdi_setProtectedProperty(mjs, channel, DEF_STR_FADE,
                mjs_mk_foreign_func(mjs, (mjs_func_ptr_t) thingjsSmartLEDSetFadeTimeAndStart));
        mjs_array_push(mjs, channels, channel);
    }
    //Append the channels array
    stdi_setProtectedProperty(mjs, interface, DEF_STR_CHANNELS, channels);

    //Return mJS interface object
    return interface;
}

void thingjsSmartLEDRegister(void) {
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
            .constructor    = thingjsSmartLEDConstructor,
            .destructor     = NULL,
            .cases          = thingjs_ledc_cases
    };

    thingjsRegisterInterface(&interface);
}



