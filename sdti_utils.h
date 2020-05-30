/*
 *  Created on: 21 мар. 2019 г.
 *      Author: rpiontik
 */

#ifndef STDI_UTILS_H_
#define STDI_UTILS_H_

#include <mjs.h>

#define NON				-1
#define ENUM			-2 /* Enumerations of pins */
#define NO_ANY   		-3 /* No any pin to use */

#define DEF_CASES(...) {__VA_ARGS__, NON}
#define DEF_CASE(...) __VA_ARGS__, NON
#define DEF_ENUM(...) ENUM, __VA_ARGS__, NON


mjs_err_t  stdi_setProtectedProperty(struct mjs *mjs, mjs_val_t interface, const char *name, mjs_val_t val);


#endif /* STDI_UTILS_H_ */
