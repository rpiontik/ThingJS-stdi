//
// Created by rpiontik on 19.01.20.
//

#include "stdi_utils.h"

//Append protected property to mjs object
void stdi_setProtectedProperty(struct mjs *mjs, mjs_val_t interface, const char *name, mjs_val_t val) {
    mjs_set_protected(mjs, interface, name, ~0, false);
    mjs_err_t result = mjs_set(mjs, interface, name, ~0, val);
    mjs_set_protected(mjs, interface, name, ~0, true);
    return result;
}
