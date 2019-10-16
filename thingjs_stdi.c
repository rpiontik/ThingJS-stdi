/*
 *  Created on: 5 февр. 2019 г.
 *      Author: rpiontik
 */

#include "thingjs_stdi.h"

#include "implementation/tgsi_1wire.h"
#include "implementation/tgsi_ledc.h"

void thingjsSTDIRegister(void) {
    thingjs1WireRegister();
    thingjsLEDCRegister();
}
