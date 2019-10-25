/*
 *  Created on: 5 февр. 2019 г.
 *      Author: rpiontik
 */

#include "thingjs_stdi.h"

#include "implementation/tgsi_1wire.h"
#include "implementation/tgsi_ledc.h"
#include "implementation/tgsi_bit_port.h"

void thingjsSTDIRegister(void) {
    thingjs1WireRegister();
    thingjsLEDCRegister();
    thingjsBitPortRegister();
}