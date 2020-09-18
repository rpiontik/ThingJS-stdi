/*
 *  Created on: 21 мар. 2019 г.
 *      Author: rpiontik
 */

#include "tgsi_sys_info.h"
#include "string.h"

#include "sdti_utils.h"
#include "thingjs_board.h"
#include "thingjs_core.h"

#define  INTERFACE_NAME "sys_info"
const char TAG_SYS_INFO[] = INTERFACE_NAME;

char * chip_id = "";

mjs_val_t thingjsSysInfoConstructor(struct mjs *mjs, cJSON *params) {
    //Validate preset params
    //The params must have pin number
    if (!cJSON_IsNumber(params))
        return MJS_UNDEFINED;

    //Create mjs object
    mjs_val_t interface = mjs_mk_object(mjs);

    //Add protected property to interface
    stdi_setProtectedProperty(mjs, interface, "idf_version",
                              mjs_mk_string(mjs, esp_get_idf_version(), ~0, 1)
                              );

    stdi_setProtectedProperty(mjs, interface, "chip_id",
                              mjs_mk_string(mjs, chip_id, ~0, 1)
    );

    //Return mJS interface object
    return interface;
}

void thingjsMakeChipID() {
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    asprintf(&chip_id, "%s-%02X%02X%02X", "TJS", mac[3], mac[4], mac[5]);
}

void thingjsSysInfoRegister(void) {
    thingjsMakeChipID();

    static int thingjs_timer_cases[] = DEF_CASES(DEF_CASE(RES_VIRTUAL));

    static const struct st_thingjs_interface_manifest interface = {
            .type           = INTERFACE_NAME,
            .constructor    = thingjsSysInfoConstructor,
            .destructor     = NULL,
            .cases          = thingjs_timer_cases
    };

    thingjsRegisterInterface(&interface);
}