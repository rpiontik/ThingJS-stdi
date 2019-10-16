/*
 *  Created on: 21 мар. 2019 г.
 *      Author: rpiontik
 */

#include "1wire.h"

#include "sdti_utils.h"

#include "thingjs_board.h"
#include "thingjs_core.h"

int thingjs_1wire_cases[] = DEF_CASES(
		DEF_CASE(GPIO17, GPIO16),	DEF_CASE(GPIO16, GPIO17),
		DEF_CASE(
				DEF_ENUM(GPIO15, GPIO2, GPIO4, GPIO1),
				DEF_ENUM(GPIO15, GPIO2, GPIO4, GPIO1)
		)
);

mjs_val_t thingjs1WireConstructor(struct mjs * mjs, cJSON * params);

const struct st_thingjs_interface_manifest thingjs_1wire_interface = {
        .type			= "1wire",
        .constructor	= thingjs1WireConstructor,
        .cases			= thingjs_1wire_cases
};

mjs_val_t thingjs1WireConstructor(struct mjs * mjs, cJSON * params) {
	return (mjs_val_t)0;
}

void thingjs1WireRegister(void) {

}



