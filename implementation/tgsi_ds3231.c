//
// Created by rpiontik on 19.01.20.
//

#include "tgsi_ds3231.h"

#include "freertos/FreeRTOSConfig.h"
#include "freertos/FreeRTOS.h"
#include <freertos/timers.h>
#include <esp_log.h>
#include <sys/time.h>
#include "driver/i2c.h"

#include "sdti_utils.h"
#include "thingjs_board.h"
#include "thingjs_core.h"

#define  INTERFACE_NAME "DS3231"

const char TAG_DS3231[] = INTERFACE_NAME;

static const int rtc_i2c_port = I2C_NUM_0;

#define DS3231_ADDR                         0x68             /*!< slave address for DS3231 RTC */
#define WRITE_BIT                           I2C_MASTER_WRITE /*!< I2C master write */
#define READ_BIT                            I2C_MASTER_READ  /*!< I2C master read */
#define ACK_CHECK_EN                        0x1              /*!< I2C master will check ack from slave*/
#define ACK_CHECK_DIS                       0x0              /*!< I2C master will not check ack from slave */
#define ACK_VAL                             0x0              /*!< I2C ack value */
#define NACK_VAL                            0x1              /*!< I2C nack value */

#define DEF_RTC_SDA                         GPIO15
#define DEF_RTC_SCL                         GPIO12

inline uint8_t bcd2dec(uint8_t b) {
    return ((b/16 * 10) + (b % 16));
}

inline uint8_t dec2bcd(uint8_t d) {
    return ((d/10 * 16) + (d % 10));
}

static esp_err_t rtc_i2c_read(i2c_port_t i2c_num, uint8_t addr, uint8_t *data_rd, size_t size) {
    if (size == 0) {
        return ESP_OK;
    }
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (DS3231_ADDR << 1) | WRITE_BIT, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, addr, ACK_CHECK_EN);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (DS3231_ADDR << 1) | READ_BIT, ACK_CHECK_EN);
    if (size > 1) {
        i2c_master_read(cmd, data_rd, size - 1, ACK_VAL);
    }
    i2c_master_read_byte(cmd, data_rd + size - 1, NACK_VAL);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(i2c_num, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    return ret;
}

static esp_err_t rtc_i2c_write(i2c_port_t i2c_num, uint8_t addr, uint8_t *data_wr, size_t size) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (DS3231_ADDR << 1) | WRITE_BIT, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, addr, ACK_CHECK_EN);
    i2c_master_write(cmd, data_wr, size, ACK_CHECK_EN);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(i2c_num, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    return ret;
}

esp_err_t rtcGetTime(time_t *ptime) {
    uint8_t rtc[7] = {0};

    esp_err_t result = rtc_i2c_read(rtc_i2c_port, 0, rtc, 7);

    if (ESP_OK != result) {
        return result;
    }

    struct tm _tm;
    _tm.tm_sec = bcd2dec(rtc[0]);
    _tm.tm_min = bcd2dec(rtc[1]);
    _tm.tm_hour = bcd2dec(rtc[2]);
    _tm.tm_mday = bcd2dec(rtc[4]);
    _tm.tm_mon = bcd2dec(rtc[5] & 0x1F) - 1; // returns 1-12
    _tm.tm_year = bcd2dec(rtc[6]) + 100;
    _tm.tm_wday = bcd2dec(rtc[3]); // returns 1-7

    *ptime = mktime((struct tm *) &_tm);

    return result;
}

esp_err_t rtcSetTime(time_t * loc_time) {
    struct tm *_time;
    _time = localtime(loc_time);

    uint8_t wrBuf[7] = {0};

    wrBuf[0] = dec2bcd(_time->tm_sec);
    wrBuf[1] = dec2bcd(_time->tm_min);
    wrBuf[2] = dec2bcd(_time->tm_hour);
    wrBuf[3] = dec2bcd(_time->tm_wday);
    wrBuf[4] = dec2bcd(_time->tm_mday);
    wrBuf[5] = dec2bcd(_time->tm_mon + 1) | 0x80;
    wrBuf[6] = dec2bcd(_time->tm_year - 100);

    esp_err_t result = rtc_i2c_write(rtc_i2c_port, 0, wrBuf, 7);

    return result;
}

//Read from RTC module current time
//if RTC presented
inline esp_err_t time_syncFromRTC(void) {
    time_t ptime;
    const esp_err_t result = rtcGetTime (&ptime);
    if (ESP_OK == result)	{
        struct timeval now = { .tv_sec = ptime};
        settimeofday(&now, NULL);
    } else {
        ESP_LOGD(TAG_DS3231, "RTC modules not presented");
    }
    return result;
}

void rtcInit(void) {
    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = DEF_RTC_SDA;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_io_num = DEF_RTC_SCL;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = 100000;
    i2c_param_config(rtc_i2c_port, &conf);
    i2c_driver_install(rtc_i2c_port, conf.mode, 0, 0, 0);

    uint8_t buf[2] = {0b00000000, 0b00001000};
    rtc_i2c_write(rtc_i2c_port, 0x0E, buf, 2);

    time_syncFromRTC();
}

// Try to get time from RTC module
static void thingjsGetTime(struct mjs *mjs) {
    time_t time;
    if(rtcGetTime(&time) == ESP_OK) {
        mjs_return(mjs, mjs_mk_number(mjs, time));
    } else {
        mjs_set_errorf(mjs, MJS_INTERNAL_ERROR, "%s: Can not give RTC time", TAG_DS3231);
        mjs_return(mjs, MJS_INTERNAL_ERROR);
    }
}

// Try to set time to RTC module
static void thingjsSetTime(struct mjs *mjs) {
    time_t now = {0};
    mjs_val_t arg0 = mjs_arg(mjs, 0);   //Target time
    // If time is empty, will get time from system
    if (mjs_is_undefined(arg0)) {
        time(&now);
    // else check param0 as number
    } else if (!mjs_is_number(arg0)) {
        mjs_set_errorf(mjs, MJS_INTERNAL_ERROR, "%s: Error of params setTime(int/undef)", TAG_DS3231);
        mjs_return(mjs, MJS_INTERNAL_ERROR);
        return;
    }

    // Try to set time
    if(rtcSetTime(&time) == ESP_OK) {
        // Try to get real time from RTC module
        thingjsGetTime(mjs);
    } else {
        mjs_set_errorf(mjs, MJS_INTERNAL_ERROR, "%s: Can not give RTC time", TAG_DS3231);
        mjs_return(mjs, MJS_INTERNAL_ERROR);
    }
}

// Try to pull system time to RTC module
static void thingjsPullTimeFromRTC(struct mjs *mjs) {
    if (ESP_OK != time_syncFromRTC()) {
        mjs_set_errorf(mjs, MJS_INTERNAL_ERROR, "%s: Can not get time from RTC module", TAG_DS3231);
        mjs_return(mjs, MJS_INTERNAL_ERROR);
    } else
        mjs_return(mjs, MJS_OK);
}

mjs_val_t thingjsDS3231Constructor(struct mjs *mjs, cJSON *params) {
    //Validate preset params
    //The params must have timer resource
    if (!cJSON_IsArray(params) || !(cJSON_GetArraySize(params) == 2)) {
        mjs_set_errorf(mjs, MJS_INTERNAL_ERROR, "%s: Incorrect params", TAG_DS3231);
        mjs_return(mjs, MJS_INTERNAL_ERROR);
        return MJS_UNDEFINED;
    }

    cJSON * sda = cJSON_GetArrayItem(params, 0);
    cJSON * scl = cJSON_GetArrayItem(params, 1);

    if (!cJSON_IsNumber(sda) || !cJSON_IsNumber(scl) ) {
        mjs_set_errorf(mjs, MJS_INTERNAL_ERROR, "%s: Incorrect params", TAG_DS3231);
        mjs_return(mjs, MJS_INTERNAL_ERROR);
        return MJS_UNDEFINED;
    }

    //Create mjs object
    mjs_val_t interface = mjs_mk_object(mjs);

    //Create timers collection
    stdi_setProtectedProperty(mjs, interface, "$sda", mjs_mk_number(mjs, sda->valueint));
    stdi_setProtectedProperty(mjs, interface, "$scl", mjs_mk_number(mjs, scl->valueint));

    //Bind functions
    stdi_setProtectedProperty(mjs, interface, "set",
            mjs_mk_foreign_func(mjs, (mjs_func_ptr_t) thingjsSetTime));
    stdi_setProtectedProperty(mjs, interface, "get",
            mjs_mk_foreign_func(mjs, (mjs_func_ptr_t) thingjsGetTime));
    stdi_setProtectedProperty(mjs, interface, "sync",
                              mjs_mk_foreign_func(mjs, (mjs_func_ptr_t) thingjsPullTimeFromRTC));

    //Return mJS interface object
    return interface;
}

void thingjsDS3231Register(void) {
    rtcInit();

    static int thingjs_ds3231_cases[] = DEF_CASES(
            //          SDA           SCL
            DEF_CASE(DEF_RTC_SDA, DEF_RTC_SCL)
    );

    static const struct st_thingjs_interface_manifest interface = {
            .type           = INTERFACE_NAME,
            .constructor    = thingjsDS3231Constructor,
            .destructor     = NULL,
            .cases          = thingjs_ds3231_cases
    };

    thingjsRegisterInterface(&interface);
}
