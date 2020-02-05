/*
 *  Created on: 21 мар. 2019 г.
 *      Author: rpiontik
 */

#include "tgsi_ledc.h"

#include "driver/gpio.h"

#include "sdti_utils.h"
#include "thingjs_board.h"
#include "thingjs_core.h"

const char TAG_LEDC[] = "LEDC";

typedef struct {
    struct mjs * process;
    mjs_val_t interface;
} st_bind_channel;

static st_bind_channel ledc_binded_channels[LEDC_NUMBER_CHANNELS] = {0};

//Flag of started LEDC
static bool ledc_inited = false;

typedef struct ledcConfigChannel_s {
    int GPIO_Number;
    uint32_t Duty;
    bool Inverce;
} ledcConfigChannel_t;

typedef struct ledcConfig_s {
    uint32_t ledcTimersResolution;
    uint32_t ledcTimersFrequency;
    ledcConfigChannel_t channels[16];
} ledcConfig_t;

static ledcConfig_t* ledcConfig;

static esp_err_t ledc_timer_reconfig(uint32_t freq, ledc_timer_bit_t resolution)
{
    /*
     * Prepare and set configuration of timers
     * that will be used by LED Controller
     */
    ledc_timer_config_t ledc_timer = {
            .duty_resolution = resolution, 			// resolution of PWM duty
            .freq_hz = freq,                    	// frequency of PWM signal
            .speed_mode = LEDC_HIGH_SPEED_MODE, 	// timer mode
            .timer_num = LEDC_TIMER_0            	// timer index
    };

    // Set configuration of timer0 for high speed channels
    esp_err_t result = ledc_timer_config(&ledc_timer);
    if ( ESP_OK != result ) {
        ESP_LOGE(TAG_LEDC, "High speed timer config error %X res:%u freq:%u", result, resolution, freq);
        return result;
    }

    // Prepare and set configuration of timer1 for low speed channels
    ledc_timer.speed_mode = LEDC_LOW_SPEED_MODE;
    ledc_timer.timer_num = LEDC_TIMER_1;

    result = ledc_timer_config(&ledc_timer);
    if ( ESP_OK != result ) {
        ESP_LOGE(TAG_LEDC, "Low speed timer config error %X res:%u freq:%u", result, resolution, freq);
        return result;
    }

    ledcConfig->ledcTimersResolution = resolution;
    ledcConfig->ledcTimersFrequency  = ledc_get_freq(LEDC_HIGH_SPEED_MODE, LEDC_TIMER_0);

    ESP_LOGD(TAG_LEDC, "Timers configured res:%u freq:%u", resolution, freq);

    return ESP_OK;
}


//Detect using ledc
static int ledc_touch(void){
    if(!ledc_inited) {

        ledcConfig = malloc(sizeof(ledcConfig_t));

        if (ledcConfig == NULL) {
            ESP_LOGE(TAG_LEDC, "Could not allocate memory for LEDC Config");
            return ESP_ERR_NO_MEM;
        }

        memset(ledcConfig, 0, sizeof(ledcConfig_t));

        ledcConfig->ledcTimersFrequency  = LEDC_CONFIG_DEFAULT_FREQUENCY;
        ledcConfig->ledcTimersResolution = LEDC_CONFIG_DEFAULT_RESOLUTION;

        //uint32_t i;
        for ( uint32_t i=0; i < LEDC_NUMBER_CHANNELS; i++) ledcConfig->channels[i].GPIO_Number = -1;

        ledc_timer_reconfig(ledcConfig->ledcTimersFrequency, ledcConfig->ledcTimersResolution);

        ledc_inited = 1;
    }
    return 0;
}


/*
 * Set individual configuration
 * for channel of LED Controller
 * by selecting:
 * - channel number ( 0-15 )
 * - output duty cycle, set initially to 0
 * - GPIO number where LED is connected to
 *   Note: if different channels use one timer,
 *         then frequency and bit_num of these channels
 *         will be the same
 */
static esp_err_t hwi_bind_ledc_channel_to_GPIO(uint32_t channel, int gpioNum, bool inverce) {
    ledc_channel_config_t ledc_channel;

    memset(&ledc_channel, 0, sizeof(ledc_channel_config_t));

    if (( channel < 8 ) && (gpioNum != 0)) {
        ledc_channel.channel    = channel;
        ledc_channel.speed_mode = LEDC_HIGH_SPEED_MODE;
        ledc_channel.timer_sel  = LEDC_TIMER_0;
    } else if ((channel < 16) && (gpioNum != 0)) {
        ledc_channel.channel    = channel - 8;
        ledc_channel.speed_mode = LEDC_LOW_SPEED_MODE;
        ledc_channel.timer_sel  = LEDC_TIMER_1;
    } else
        return ESP_ERR_INVALID_ARG;

    ledc_channel.duty = inverce ? (1 << ledcConfig->ledcTimersResolution) - 1 : 0;
    ledc_channel.gpio_num   = gpioNum;

    esp_err_t result = ledc_channel_config(&ledc_channel);

    if(ESP_OK == result)
    {
        ledcConfig->channels[channel].GPIO_Number = ledc_channel.gpio_num;
        ledcConfig->channels[channel].Duty        = ledc_channel.duty;
        ledcConfig->channels[channel].Inverce     = inverce;
    }

    return result;
}

static void hwi_bind_ledc_interface(struct mjs *mjs, mjs_val_t interface) {
    mjs_val_t channel = mjs_get(mjs, interface, "channel", ~0);
    mjs_val_t gpio = mjs_get(mjs, interface, "gpio", ~0);
    mjs_val_t inverce = mjs_get(mjs, interface, "inverce", ~0);

    hwi_bind_ledc_channel_to_GPIO(
            mjs_get_int(mjs, channel),
            mjs_get_int(mjs, gpio),
            !!mjs_get_bool(mjs, inverce)
    );
}

static int hwi_bind_channel(struct mjs *mjs, mjs_val_t interface) {
    for(int f=0; f < LEDC_NUMBER_CHANNELS; f++) {
        st_bind_channel * channel = &ledc_binded_channels[f];
        if(!channel->process) {
            channel->process = mjs;
            channel->interface = interface;
            return f;
        }
    }
    return -1;
}

//Interface function
static void hwi_vm_func_setDutyToChannelWithFade(struct mjs *mjs) {
    mjs_val_t res = MJS_UNDEFINED;
    //Get function params
    mjs_val_t arg0 = mjs_arg(mjs, 0);
    mjs_val_t arg1 = mjs_arg(mjs, 1);
    //Validate params
    if(mjs_is_number(arg0) && mjs_is_number(arg1)) {
        //Get this object that store params
        mjs_val_t this_obj = mjs_get_this(mjs);
        //Get internal params
        mjs_val_t params = mjs_get(mjs, this_obj, "gpio", ~0);
        //Validate internal params
        if(mjs_is_number(arg0)) {
            //Convert internal params
            uint32_t channel = mjs_get_int32(mjs, params);
            //Convert function params
            uint32_t duty = mjs_get_int32(mjs, arg0);
            uint32_t fade_ms = mjs_get_int32(mjs, arg1);

            uint32_t speedMode = LEDC_HIGH_SPEED_MODE;
            uint32_t ch = channel;

            if (channel < 8)
                speedMode = LEDC_HIGH_SPEED_MODE;
            else if (channel < 16) {
                ch = channel - 8;
                speedMode = LEDC_LOW_SPEED_MODE;
            } else {
                mjs_set_errorf(mjs, MJS_INTERNAL_ERROR, "%s: Incorrect internal param channel", TAG_LEDC);
                res = MJS_INTERNAL_ERROR;
            }

            uint32_t dutyToSet = duty;
            if( ledcConfig->channels[channel].Inverce )  dutyToSet = ( 1 << ledcConfig->ledcTimersResolution ) - duty - 1;

            dutyToSet &= 0xFFFF;

            ESP_LOGD(TAG_LEDC, "setDutyToChannelWithFade(%d, %d, %d)", channel, duty, fade_ms);

            esp_err_t result = ledc_set_fade_time_and_start(speedMode,ch,dutyToSet,fade_ms,LEDC_FADE_NO_WAIT);

            if (ESP_OK != result) res = MJS_INTERNAL_ERROR;
            else res = MJS_OK;

        } else {
            mjs_set_errorf(mjs, MJS_INTERNAL_ERROR, "%s: Incorrect internal params", TAG_LEDC);
            res = MJS_INTERNAL_ERROR;
        }
    } else {
        mjs_set_errorf(mjs, MJS_SYNTAX_ERROR, "Error params of function setDutyToChannelWithFade(int, int, int)");
        res = MJS_SYNTAX_ERROR;
    }
    mjs_return(mjs, res);
}

mjs_val_t thingjsLEDCConstructor(struct mjs *mjs, cJSON *params) {
    //Validate preset params
    if(!cJSON_IsNumber(params))
        return MJS_UNDEFINED;

    //Call LEDC configurator
    if( 0!=ledc_touch()) return MJS_UNDEFINED;

    //Create mJS interface object
    mjs_val_t interface = mjs_mk_object(mjs);

    //Try to bind channel number
    int channel = hwi_bind_channel(mjs, interface);
    if(channel < 0) {
        ESP_LOGE(TAG_LEDC, "Can not bind channel %d", channel);
        return MJS_UNDEFINED;
    }

    //Add protected property to interface
    mjs_set(mjs, interface, "channel", ~0, mjs_mk_number(mjs, channel));
    mjs_set(mjs, interface, "gpio", ~0, mjs_mk_number(mjs, params->valueint));
    mjs_set(mjs, interface, "duty", ~0, mjs_mk_number(mjs, (1 << ledcConfig->ledcTimersResolution) - 1));
    mjs_set(mjs, interface, "inverce", ~0, mjs_mk_boolean(mjs, 1));

    //Set protected flag
    mjs_set_protected(mjs, interface, "channel", ~0, true);
    mjs_set_protected(mjs, interface, "gpio", ~0, true);
    mjs_set_protected(mjs, interface, "duty", ~0, true);
    mjs_set_protected(mjs, interface, "inverce", ~0, true);

    //Bind functions
    mjs_set(mjs, interface, "setDutyToChannelWithFade", ~0, mjs_mk_foreign_func(mjs, (mjs_func_ptr_t) hwi_vm_func_setDutyToChannelWithFade));

    //Bind in LED controller
    hwi_bind_ledc_interface(mjs, interface);

    //Return mJS interface object
    return interface;
}

void thingjsLEDCRegister(void) {
    static int thingjs_ledc_cases[] = DEF_CASES(
            DEF_CASE(GPIO0),  DEF_CASE(GPIO2), DEF_CASE(GPIO3), DEF_CASE(GPIO4),
            DEF_CASE(GPIO5),  DEF_CASE(GPIO12), DEF_CASE(GPIO13), DEF_CASE(GPIO14),
            DEF_CASE(GPIO15), DEF_CASE(GPIO16), DEF_CASE(GPIO17), DEF_CASE(GPIO18),
            DEF_CASE(GPIO19), DEF_CASE(GPIO21), DEF_CASE(GPIO22), DEF_CASE(GPIO23),
            DEF_CASE(GPIO25), DEF_CASE(GPIO26), DEF_CASE(GPIO27), DEF_CASE(GPIO32),
            DEF_CASE(GPIO33)
    );

    static const struct st_thingjs_interface_manifest interface = {
            .type           = "ledc",
            .constructor    = thingjsLEDCConstructor,
            .destructor     = NULL,
            .cases          = thingjs_ledc_cases
    };

    thingjsRegisterInterface(&interface);
}



