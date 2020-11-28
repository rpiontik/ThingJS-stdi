/*
 *  Created on: 5 февр. 2019 г.
 *      Author: rpiontik
 */

#include "thingjs_stdi.h"

#include "implementation/tgsi_bit_port.h"
#include "implementation/tgsi_timer.h"
#include "implementation/tgsi_clock.h"
#include "implementation/tgsi_ds3231.h"
#include "implementation/tgsi_smart_led.h"
#include "implementation/tgsi_http.h"
#include "implementation/tgsi_ds18x20.h"
#include "implementation/tgsi_mqttc.h"
#include "implementation/tgsi_sys_info.h"
#include "implementation/tgsi_pref.h"
#include "implementation/tgsi_dac.h"
#include "implementation/tgsi_adc.h"
#include "implementation/tgsi_pcnt.h"

void thingjsSTDIRegister(void) {
    thingjsBitPortRegister();
    thingjsTimersRegister();
    thingjsClockRegister();
    thingjsDS3231Register();
    thingjsSmartLEDRegister();
    thingjsHTTPRegister();
    thingjsDS18X20Register();
    thingjsMQTTRegister();
    thingjsSysInfoRegister();
    thingjsPrefRegister();
    thingjsDacRegister();
    thingjsAdcRegister();
    thingjsPcntRegister();
}
