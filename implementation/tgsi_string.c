/*
 *  Created on: 17.11.2020
 *      Author: rpiontik
 */

#include <esp_log.h>
#include <mongoose.h>
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
const char DEF_STR_SPLIT[] = "split";
const char DEF_STR_TEMPLATE[] = "template";
const char DEF_STR_MUSTACHE[] = "mustache";

const char DEF_OPEN_TEMPLATE[] = "{{";
const char DEF_CLOSE_TEMPLATE[] = "}}";

static char * thingjsCoreToString(struct mjs *mjs, mjs_val_t context) {
    char * result = NULL;
    if(mjs_is_undefined(context)) {
        const char udf[] = "undefined";
        result = malloc(10);
        memcpy(result, udf, 10);
    } else if(mjs_is_string(context)) {
        size_t len;
        mjs_val_t tmp = context;
        const char * buf = mjs_get_string(mjs, &tmp, &len);
        result = malloc(len+1);
        memcpy(result, buf, len);
        result[len] = '\0';
    } else if(mjs_is_object(context)) {
        const char obj[] = "[object Object]";
        result = malloc(16);
        memcpy(result, obj, 16);
    } else if(mjs_is_array(context)) {
        //todo нужно реализовать
    } else {
        mjs_json_stringify(mjs, context, NULL, 0, &result);
    }
    return result;
}

static mjs_val_t thingjsToString(struct mjs *mjs) {
    char * str = thingjsCoreToString(mjs, mjs_arg(mjs, 0));
    mjs_val_t result = MJS_OK;
    if(str) {
        mjs_return(mjs, mjs_mk_string(mjs, str, ~0, 1));
        free(str);
    } else
        result = MJS_INTERNAL_ERROR;

    return result;
}

static mjs_val_t thingjsReplaceAll(struct mjs *mjs) {
    //Based on https://www.geeksforgeeks.org/c-program-replace-word-text-another-given-word/
    char * string = thingjsCoreToString(mjs, mjs_arg(mjs, 0)); //Base string
    char * oldW = thingjsCoreToString(mjs, mjs_arg(mjs, 1)); //Old word
    char * newW = thingjsCoreToString(mjs, mjs_arg(mjs, 2)); //New word
    if(!string || !oldW || !newW){
        if(string) free(string);
        if(oldW) free(oldW);
        if(newW) free(newW);
        mjs_return(mjs, MJS_INTERNAL_ERROR);
        return MJS_INTERNAL_ERROR;
    }

    char* result;
    char* str = string;
    int i, cnt = 0;
    int newWlen = strlen(newW);
    int oldWlen = strlen(oldW);

    // Counting the number of times old word
    // occur in the string
    for (i = 0; str[i] != '\0'; i++) {
        if (!oldWlen || (strstr(&str[i], oldW) == &str[i])) {
            cnt++;
            // Jumping to index after the old word.
            i += oldWlen - (!oldWlen ? 0 : 1);
        }
    }

    // Making new string of enough length
    result = (char*)malloc(i + cnt * (newWlen - oldWlen) + 1);

    i = 0;
    while (*str) {
        // compare the substring with the result
        if(!oldWlen) {
            strcpy(&result[i], newW);
            i += newWlen;
            result[i++] = *str++;
        } else if ((strstr(str, oldW) == str)) {
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
    free(string);

    mjs_return(mjs, mjs_mk_string(mjs, result, i, 1));

    free(result);

    return MJS_OK;
}

static mjs_val_t thingjsSplit(struct mjs *mjs) {
    char * string = thingjsCoreToString(mjs, mjs_arg(mjs, 0)); //Base string
    char * split = thingjsCoreToString(mjs, mjs_arg(mjs, 1)); //Old word
    int split_len = strlen(split);
    mjs_val_t limit_ = mjs_arg(mjs, 2);
    int limit = mjs_is_number(limit_) ? mjs_get_int(mjs, limit_) : -1;
    mjs_val_t result = mjs_mk_array(mjs);

    int i = 0;
    int start = 0;
    for(; (limit != 0) && string[i]; i++) {
        if ((strstr(&string[i], split) == &string[i])) {
            mjs_array_push(mjs, result, mjs_mk_string(mjs, &string[start], i - start, 1));
            start = i + split_len;
            i = start - 1;
            if(limit > 0) limit--;
        }
    }

    if(limit != 0) {
        mjs_array_push(mjs, result, mjs_mk_string(mjs, &string[start], i - start, 1));
    }

    free(split);
    free(string);

    mjs_return(mjs, result);

    return MJS_OK;
}

struct str {
    char * buffer;
    int length;
};

static void appendString(struct str * str, char * append, int length) {
    str->buffer = realloc(str->buffer, str->length + length);
    memcpy(&str->buffer[str->length], append, length);
    str->length += length;
}

static mjs_val_t thingjsTemplate(struct mjs *mjs) {
    char * template = thingjsCoreToString(mjs, mjs_arg(mjs, 0)); //Template
    int i = 0;
    int start = -1;
    int offset = 0;
    struct str result = {
            .length = 0,
            .buffer = malloc(1)
    };
    char * script = malloc(1);
    int script_buf_size = 0;
    for(; template[i]; i++) {
        if((start == -1) && (*(uint16_t*)DEF_OPEN_TEMPLATE) == (*(uint16_t*)&template[i])) {
            appendString(&result, &template[offset], i - offset);
            start = i + 2;
        } else if((start > -1) && (*(uint16_t*)DEF_CLOSE_TEMPLATE) == (*(uint16_t*)&template[i])) {
            int script_length = i - start;
            if(script_buf_size < script_length) {
                script_buf_size = script_length + 4;
                script = realloc(script, script_buf_size);
            }
            snprintf(script, script_buf_size, "(%.*s);", script_length, &template[start]);

            mjs_val_t res = MJS_UNDEFINED;
            mjs_eval(mjs, script, &res);

            char * eval_result = thingjsCoreToString(mjs, res);
            appendString(&result, eval_result, strlen(eval_result));

            free(eval_result);
            offset = i + 2;
            start = -1;
        }
    }

    appendString(&result, &template[offset], i - offset);

    mjs_return(mjs, mjs_mk_string(mjs, result.buffer, result.length, 1));
    free(template);
    free(script);
    free(result.buffer);

    return MJS_OK;
}

static int32_t thingjsMustacheParse(
        struct mjs *mjs, mjs_val_t context,
        struct mbuf *mbuf,
        const char * template, uint16_t offset,
        const char * sec_name, int sec_name_len,
        bool visible) {
    int mustache_start = -1;
    int part_start = offset;
    int i = offset;
    for(; template[i]; i++) {
        if((mustache_start == -1) && (*(uint16_t*)DEF_OPEN_TEMPLATE) == (*(uint16_t*)&template[i])) {
            if(visible) mbuf_append(mbuf, &template[part_start], i - part_start);
            mustache_start = i + 2;
        } else if((mustache_start > -1) && (*(uint16_t*)DEF_CLOSE_TEMPLATE) == (*(uint16_t*)&template[i])) {
            enum {st_begin_pos, st_begin_neg, st_exp} mustache_type = st_exp;
            int mustache_len = i - mustache_start;
            switch(template[mustache_start]) {
                case '#':
                    mustache_type = st_begin_pos;
                    mustache_start++;
                    mustache_len--;
                    break;
                case '^':
                    mustache_type = st_begin_neg;
                    mustache_start++;
                    mustache_len--;
                    break;
                case '/':
                    mustache_start++;
                    if(!sec_name || (strncmp(sec_name, &template[mustache_start], sec_name_len) != 0)) {
                        mjs_return(mjs, MJS_INTERNAL_ERROR);
                        mjs_set_errorf(mjs, MJS_INTERNAL_ERROR, "%s: Mustache syntax error at %d char for section [%.*s]",
                                       pcTaskGetTaskName(NULL), i, sec_name_len, sec_name ? sec_name : "");
                        return -1;
                    }
                    return i + 2;
                case '!':
                    mustache_start = -1;
                    part_start = i + 1;
                    i++;
                    continue;
            }
            mjs_val_t value = mjs_get(mjs, context, &template[mustache_start], mustache_len);
            if(mjs_is_array(value)) {
                int jump = -1;
                for(int l=0; l < mjs_array_length(mjs, value); l++) {
                    jump = thingjsMustacheParse(mjs, mjs_array_get(mjs, value, l), mbuf,
                                                        template, i + 2,
                                                        &template[mustache_start], mustache_len,
                                                        visible
                                                );
                }
                if(jump<0) return -1;
                i = jump - 1;
            } else if (mustache_type == st_exp) {
                if(visible && !mjs_is_undefined(value)) {
                    char *exp = thingjsCoreToString(mjs, value);
                    mbuf_append(mbuf, exp, strlen(exp));
                    free(exp);
                }
                i++;
            } else {
                ESP_LOGD(TAG_STRING, "Section [%.*s] type of %s as int %d", mustache_len, &template[mustache_start], mjs_typeof(value), mjs_get_int(mjs, value));
                int32_t jump = thingjsMustacheParse(mjs, mjs_is_boolean(value) ? context : value, mbuf,
                                    template, i + 2,
                                    &template[mustache_start], mustache_len,
                                    visible && (
                                            mjs_is_object(value)
                                            || mjs_get_bool(mjs, value)
                                            || (mjs_is_number(value) && (mjs_get_int(mjs, value) != 0)))
                                );
                if(jump < 0) return -1;
                i = jump - 1;
            }
            mustache_start = -1;
            part_start = i + 1;
        }
    }

    if(visible) mbuf_append(mbuf, &template[part_start], i - part_start);

    return i;
}

static mjs_val_t thingjsMustache(struct mjs *mjs) {
    char * template = thingjsCoreToString(mjs, mjs_arg(mjs, 0)); //Template
    struct mbuf buff = {0};
    mbuf_init(&buff, 1);
    buff.buf[0] = '\0';

    if(thingjsMustacheParse(mjs, mjs_arg(mjs, 1), &buff, template, 0, NULL, 0, true) >= 0) {
        mjs_return(mjs, mjs_mk_string(mjs, buff.buf, buff.len, 1));
    }

    mbuf_free(&buff);

    return MJS_OK;
}

mjs_val_t thingjsStringConstructor(struct mjs *mjs, cJSON *params) {
    mjs_val_t this = mjs_mk_object(mjs);
    stdi_setProtectedProperty(mjs, this, DEF_STR_TO_STRING,
                              mjs_mk_foreign_func(mjs, (mjs_func_ptr_t) thingjsToString));
    stdi_setProtectedProperty(mjs, this, DEF_STR_REPLACE_ALL,
                              mjs_mk_foreign_func(mjs, (mjs_func_ptr_t) thingjsReplaceAll));
    stdi_setProtectedProperty(mjs, this, DEF_STR_SPLIT,
                              mjs_mk_foreign_func(mjs, (mjs_func_ptr_t) thingjsSplit));
    stdi_setProtectedProperty(mjs, this, DEF_STR_TEMPLATE,
                              mjs_mk_foreign_func(mjs, (mjs_func_ptr_t) thingjsTemplate));
    stdi_setProtectedProperty(mjs, this, DEF_STR_MUSTACHE,
                              mjs_mk_foreign_func(mjs, (mjs_func_ptr_t) thingjsMustache));
    return this;
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