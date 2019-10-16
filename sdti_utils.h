/*
 *  Created on: 21 мар. 2019 г.
 *      Author: rpiontik
 */

#ifndef STDI_UTILS_H_
#define STDI_UTILS_H_

#define NON				-1
#define ENUM			-2 /* Enumerations of pins */

#define DEF_CASES(...) {__VA_ARGS__, NON}
#define DEF_CASE(...) __VA_ARGS__, NON
#define DEF_ENUM(...) ENUM, __VA_ARGS__, NON


#endif /* STDI_UTILS_H_ */
