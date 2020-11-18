/*
 *  Created on: 17.11.2020
 *      Author: rpiontik
 */

#include "tgsi_string.h"
#include "string.h"

#include "sdti_utils.h"
#include "thingjs_board.h"
#include "thingjs_core.h"

#define  INTERFACE_NAME "string"
const char TAG_STRING[] = INTERFACE_NAME;
const char DEF_STR_CONTEXT[] = "$c";
const char DEF_STR_TO_STRING[] = "toString";
const char DEF_STR_REPLACE_ALL[] = "replaceAll";


static char * thingjsCoreToString(struct mjs *mjs, mjs_val_t context) {
    char * result = NULL;
    if(mjs_is_string(context)) {
        size_t len;
        mjs_val_t tmp = context;
        const char * buf = mjs_get_string(mjs, &tmp, &len);
        result = malloc(len);
        memcpy(result, buf, len);
    } if(mjs_is_object(context)) {
        const char obj[] = "[object Object]";
        result = malloc(16);
        snprintf(result, 16, "%s", obj);
    }  if(mjs_is_array(context)) {
        //todo нужно реализовать
    } else {
        mjs_json_stringify(mjs, context, NULL, 0, &result);
    }
    return result;
}

static mjs_val_t thingjsToString(struct mjs *mjs) {
    char * str = thingjsCoreToString(mjs, mjs_get(mjs, mjs_get_this(mjs), DEF_STR_CONTEXT, ~0));
    mjs_val_t result = MJS_UNDEFINED;
    if(str) {
        result = mjs_mk_string(mjs, str, ~0, 1);
        free(str);
    }
    return result;
}

static mjs_val_t thingjsReplaceAll(struct mjs *mjs) {
    //Based on https://www.geeksforgeeks.org/c-program-replace-word-text-another-given-word/
    mjs_val_t oldW_ = mjs_arg(mjs, 0);   //Old word
    const mjs_val_t this = mjs_get_this(mjs);
    if(mjs_is_undefined(oldW_))
        return this;

    char * oldW = thingjsCoreToString(mjs, oldW_); //Old word
    char * newW = thingjsCoreToString(mjs, mjs_arg(mjs, 1)); //New word
    char * str = thingjsCoreToString(mjs, mjs_get(mjs, this, DEF_STR_CONTEXT, ~0));
    if(!str){
        free(oldW);
        free(newW);
        return MJS_INTERNAL_ERROR;
    }

    char* result;
    int i, cnt = 0;
    int newWlen = strlen(newW);
    int oldWlen = strlen(oldW);

    // Counting the number of times old word
    // occur in the string
    for (i = 0; str[i] != '\0'; i++) {
        if (strstr(&str[i], oldW) == &str[i]) {
            cnt++;
            // Jumping to index after the old word.
            i += oldWlen - 1;
        }
    }

    // Making new string of enough length
    result = (char*)malloc(i + cnt * (newWlen - oldWlen) + 1);

    i = 0;
    while (*str) {
        // compare the substring with the result
        if (strstr(str, oldW) == str) {
            strcpy(&result[i], newW);
            i += newWlen;
            str += oldWlen;
        }
        else
            result[i++] = *str++;
    }

    result[i] = '\0';

    free(oldW);
    free(newW);
    free(str);

    const mjs_val_t ret = mjs_mk_string(mjs, result, ~0, 1);

    free(result);

    return ret;
}

static mjs_val_t thingjsString(struct mjs *mjs) {
    //Create mJS interface object
    mjs_val_t this = mjs_mk_object(mjs);
    stdi_setProtectedProperty(mjs, this, DEF_STR_CONTEXT, mjs_arg(mjs, 0));
    stdi_setProtectedProperty(mjs, this, DEF_STR_TO_STRING,
                              mjs_mk_foreign_func(mjs, (mjs_func_ptr_t) thingjsToString));
    stdi_setProtectedProperty(mjs, this, DEF_STR_REPLACE_ALL,
                              mjs_mk_foreign_func(mjs, (mjs_func_ptr_t) thingjsReplaceAll));

    return this;
}

mjs_val_t thingjsStringConstructor(struct mjs *mjs, cJSON *params) {
    return mjs_mk_foreign_func(mjs, (mjs_func_ptr_t) thingjsString);
}

void thingjsStringRegister(void) {
    static int thingjs_string_cases[] = DEF_CASES(DEF_CASE(RES_VIRTUAL));

    static const struct st_thingjs_interface_manifest interface = {
            .type           = INTERFACE_NAME,
            .constructor    = thingjsStringConstructor,
            .destructor     = NULL,
            .cases          = thingjs_string_cases
    };

    thingjsRegisterInterface(&interface);
}