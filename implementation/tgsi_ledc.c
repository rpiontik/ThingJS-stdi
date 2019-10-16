/*
 *  Created on: 21 мар. 2019 г.
 *      Author: rpiontik
 */

#include "tgsi_ledc.h"

#include "cJSON.h"
#include "mjs.h"

#include "driver/ledc.h"
#include "driver/gpio.h"
#include "driver/periph_ctrl.h"
#include "esp_log.h"
#include "string.h"

#include "sdti_utils.h"
#include "thingjs_board.h"
#include "thingjs_core.h"

//Declare available pin cases for interface
int thingjs_ledc_cases[] = DEF_CASES(
		DEF_CASE(GPIO2),	DEF_CASE(GPIO3),	DEF_CASE(GPIO4),	DEF_CASE(GPIO5),
		DEF_CASE(GPIO12),	DEF_CASE(GPIO13),	DEF_CASE(GPIO14),	DEF_CASE(GPIO15),
		DEF_CASE(GPIO16),	DEF_CASE(GPIO17),	DEF_CASE(GPIO18),	DEF_CASE(GPIO19),
		DEF_CASE(GPIO21),	DEF_CASE(GPIO22),	DEF_CASE(GPIO23),	DEF_CASE(GPIO25),
		DEF_CASE(GPIO26),	DEF_CASE(GPIO27),	DEF_CASE(GPIO32),	DEF_CASE(GPIO33)
);

mjs_val_t thingjsLEDCConstructor(struct mjs * mjs, cJSON * params);

const struct st_thingjs_interface_manifest thingjs_ledc_interface = {
        .type			= "ledc",
        .constructor	= thingjsLEDCConstructor,
        .cases			= thingjs_ledc_cases
};

void thingjsLEDCRegister(void) {

}

const char TAG_LEDC[] = "LEDC";

#define LEDC_NUMBER_CHANNELS 16

typedef struct {
	struct mjs * process;
	mjs_val_t interface;
} st_bind_channel;

static st_bind_channel ledc_binded_channels[LEDC_NUMBER_CHANNELS] = {0};

//Flag of started LEDC
static bool ledc_inited = false;

static uint32_t ledc_timers_resolution = 15;
static uint32_t ledc_timers_frequency = 2440;

/*
typedef struct {
	uint32_t mode;
    uint32_t duty_start;
    uint32_t duty_stop;
    uint32_t duty_current;
    uint32_t fade_time_ms;
	uint32_t current_time_ms;
} st_ledc_channel_fade;

static struct st_ledc_channel_fade ledc_channel_fades[LEDC_NUMBER_CHANNELS];

static void ledc_control_task(void *data) {
	ESP_LOGI(TAG_LEDC, "LED Control Task starting");

	memset(ledc_channel_fades, 0, sizeof(ledc_channel_fades));

	uint32_t channel = 0;
	uint32_t duty = 0;

	for(;;) {
		for ( channel = 0; channel < LEDC_NUMBER_CHANNELS; channel++ ) {
			st_ledc_channel_fade * channel = &ledc_channel_fades[channel];
			switch (channel->mode ) {
			case _ledcChannelModeOff:
				if ( hwConfig.channels[channel].Duty != 0 )
					_ledc_setDutyToChannel(channel, 0);
				break;
			case _ledcChannelModeOn:
				//_ledc_setDutyToChannel(channel, hwConfig.channels[channel].Duty);
			case _ledcChannelModeFadeRun:
				duty = _ledcCalculateFadeValue(
							channel->duty_start,
							channel->duty_stop,
							channel->fade_time_ms,
							channel->current_time_ms
						);
				channel->current_time_ms += 50;
				_ledc_setDutyToChannel(channel, duty);

				if (channel->current_time_ms >= channel->fade_time_ms)
					channel->mode = _ledcChannelModeOn;
				break;
			}
		}
		vTaskDelay( 50 / portTICK_PERIOD_MS );
	}
}

*/

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

    ledc_timers_resolution = resolution;
    ledc_timers_frequency  = ledc_get_freq(LEDC_HIGH_SPEED_MODE, LEDC_TIMER_0);

    ESP_LOGD(TAG_LEDC, "Timers configured res:%u freq:%u", resolution, freq);

    return ESP_OK;
}


//Detect using ledc
static void ledc_touch(void){
	if(!ledc_inited) {
		ledc_timer_reconfig(ledc_timers_frequency, ledc_timers_resolution);
		//xTaskCreatePinnedToCore(ledc_control_task, "LedController", 2000, NULL, 5, NULL, 0);
	}
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

	ledc_channel.duty = inverce ? (1 << ledc_timers_resolution) - 1 : 0;
	ledc_channel.gpio_num   = gpioNum;

	esp_err_t result = ledc_channel_config(&ledc_channel);

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

			ESP_LOGD(TAG_LEDC, "setDutyToChannelWithFade(%d, %d, %d)", channel, duty, fade_ms);
			res = MJS_OK;
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

//Create new interface instance
mjs_val_t thingjsLEDCConstructor(struct mjs * mjs, cJSON * params) {
	//Validate preset params
	if(!cJSON_IsNumber(params))
		return MJS_UNDEFINED;
	//Call LEDC configurator
	ledc_touch();
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
	mjs_set(mjs, interface, "duty", ~0, mjs_mk_number(mjs, (1 << ledc_timers_resolution) - 1));
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




