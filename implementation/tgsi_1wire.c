/*
 *  Created on: 21 мар. 2019 г.
 *      Author: rpiontik
 */

#include "tgsi_1wire.h"

#include "sdti_utils.h"

#include "thingjs_board.h"
#include "thingjs_core.h"

mjs_val_t thingjs1WireConstructor(struct mjs *mjs, cJSON *params);


mjs_val_t thingjs1WireConstructor(struct mjs *mjs, cJSON *params) {
    return (mjs_val_t) 0;
}

void thingjs1WireRegister(void) {

    static int thingjs_1wire_cases[] = DEF_CASES(
            DEF_CASE(GPIO17, GPIO16), DEF_CASE(GPIO16, GPIO17),
            DEF_CASE(
                    DEF_ENUM(GPIO15, GPIO2, GPIO4, GPIO1),
                    DEF_ENUM(GPIO15, GPIO2, GPIO4, GPIO1)
            )
    );

    static const struct st_thingjs_interface_manifest interface = {
            .type            = "1wire",
            .constructor    = thingjs1WireConstructor,
            .cases            = thingjs_1wire_cases
    };

    thingjsRegisterInterface(&interface);
}



