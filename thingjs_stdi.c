/*
 *  Created on: 5 февр. 2019 г.
 *      Author: rpiontik
 */

#include "thingjs_stdi.h"

#include "implementation/tgsi_bit_port.h"
#include "implementation/tgsi_timer.h"
#include "implementation/tgsi_ledc.h"

void thingjsSTDIRegister(void) {
    thingjsBitPortRegister();
    thingjsTimersRegister();
    thingjsLEDCRegister();
}
