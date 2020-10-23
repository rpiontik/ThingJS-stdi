/*
 *  Created on: 01 окт. 2020 г.
 *      Author: rpiontik
 */

#include "tgsi_pref.h"
#include "string.h"

#include "sdti_utils.h"
#include "thingjs_board.h"
#include "thingjs_core.h"
#include "nvs.h"
#include "esp_log.h"

#define INTERFACE_NAME "preferences"
#define SYS_PROP_CONTEXT  "$context"
const char TAG_PREFS[] = INTERFACE_NAME;
const char DEF_INCORRECT_PARAMS[] = "%s/%s: Incorrect params";

#define APPNAME pcTaskGetTaskName(NULL)

static void thingjsPrefGet(struct mjs *mjs) {
    mjs_val_t this = mjs_get_this(mjs);  //this interface object
    mjs_val_t key = mjs_arg(mjs, 0);  //NVS key
    mjs_val_t def = mjs_arg(mjs, 1);  //Default value
    mjs_val_t context = mjs_get(mjs, this, SYS_PROP_CONTEXT, ~0);

    if(mjs_is_string(key) && mjs_is_foreign(context)) {
        nvs_handle_t handle = (nvs_handle)mjs_get_ptr(mjs, context);
        const char *c_key = mjs_get_cstring(mjs, &key);
        size_t len = 0;
        char * json = NULL;
        esp_err_t err = nvs_get_str(handle, c_key, NULL, &len);
        if(!err) {
            json = malloc(len + 1);
            err = nvs_get_str(handle, c_key, json, &len);
        }
        if(err){
            mjs_return(mjs, def);
        } else {
            mjs_val_t result = MJS_UNDEFINED;
            if(mjs_json_parse(mjs, json, len, &result) == MJS_OK) {
                mjs_return(mjs, result);
            } else {
                mjs_return(mjs, def);
            }
        }

        if(json) free(json);
    } else {
        mjs_set_errorf(mjs, MJS_INTERNAL_ERROR, DEF_INCORRECT_PARAMS, APPNAME, TAG_PREFS);
    }
}

static void thingjsPrefPut(struct mjs *mjs) {
    mjs_val_t this = mjs_get_this(mjs);    //this interface object
    mjs_val_t key = mjs_arg(mjs, 0);    //NVS key
    mjs_val_t value = mjs_arg(mjs, 1);  //NVS value
    mjs_val_t context = mjs_get(mjs, this, SYS_PROP_CONTEXT, ~0);
    if(mjs_is_string(key) && mjs_is_foreign(context)) {
        char *json = NULL;
        nvs_handle handle = (nvs_handle)mjs_get_ptr(mjs, context);
        mjs_json_stringify(mjs, value, NULL, 0, &json);
        esp_err_t err = nvs_set_str(handle, mjs_get_cstring(mjs, &key), json);
        if(!err) {
            nvs_commit(handle);
        }
        if(err){
            mjs_set_errorf(mjs, MJS_INTERNAL_ERROR, "%s/%s: Cannot put NVS value", APPNAME, TAG_PREFS);
        }
    } else {
        mjs_set_errorf(mjs, MJS_INTERNAL_ERROR, DEF_INCORRECT_PARAMS, APPNAME, TAG_PREFS);
    }
}

static void thingjsPrefRemove(struct mjs *mjs) {
    mjs_val_t this = mjs_get_this(mjs);    //this interface object
    mjs_val_t key = mjs_arg(mjs, 0);    //NVS key
    mjs_val_t context = mjs_get(mjs, this, SYS_PROP_CONTEXT, ~0);
    if(mjs_is_string(key) && mjs_is_foreign(context)) {
        esp_err_t err = nvs_erase_key((nvs_handle)mjs_get_ptr(mjs, context), mjs_get_cstring(mjs, &key));
        if(err){
            mjs_set_errorf(mjs, MJS_INTERNAL_ERROR, "%s/%s: Cannot remove NVS key", APPNAME, TAG_PREFS);
        }
    } else {
        mjs_set_errorf(mjs, MJS_INTERNAL_ERROR, DEF_INCORRECT_PARAMS, APPNAME, TAG_PREFS);
    }
}

static void thingjsPrefClear(struct mjs *mjs) {
    mjs_val_t this = mjs_get_this(mjs);    //this interface object
    mjs_val_t context = mjs_get(mjs, this, SYS_PROP_CONTEXT, ~0);
    if(mjs_is_foreign(context)) {
        esp_err_t err = nvs_erase_all((nvs_handle)mjs_get_ptr(mjs, context));
        if(err){
            mjs_set_errorf(mjs, MJS_INTERNAL_ERROR, "%s/%s: Cannot erase NVS", APPNAME, TAG_PREFS);
        }
    }
}

mjs_val_t thingjsPrefConstructor(struct mjs *mjs, cJSON *params) {

    nvs_handle pref_handle;
    esp_err_t err = nvs_open(APPNAME, NVS_READWRITE, &pref_handle);

    if(err){
        mjs_set_errorf(mjs, MJS_INTERNAL_ERROR, "%s/%s: Cannot open NVS", APPNAME, TAG_PREFS);
        return MJS_INTERNAL_ERROR;
    }

    //Create mjs object
    mjs_val_t interface = mjs_mk_object(mjs);

    //Add protected property to interface
    stdi_setProtectedProperty(mjs, interface, SYS_PROP_CONTEXT, mjs_mk_foreign(mjs, (void*)pref_handle));
    stdi_setProtectedProperty(mjs, interface, "clear",
                              mjs_mk_foreign_func(mjs, (mjs_func_ptr_t) thingjsPrefClear));
    stdi_setProtectedProperty(mjs, interface, "remove",
                              mjs_mk_foreign_func(mjs, (mjs_func_ptr_t) thingjsPrefRemove));
    stdi_setProtectedProperty(mjs, interface, "put",
                              mjs_mk_foreign_func(mjs, (mjs_func_ptr_t) thingjsPrefPut));
    stdi_setProtectedProperty(mjs, interface, "get",
                              mjs_mk_foreign_func(mjs, (mjs_func_ptr_t) thingjsPrefGet));

    //Return mJS interface object
    return interface;
}

void thingjsPrefDestructor(struct mjs *mjs, mjs_val_t subject) {
    mjs_val_t this = mjs_get_this(mjs);    //this interface object
    mjs_val_t context = mjs_get(mjs, this, SYS_PROP_CONTEXT, ~0);
    if(mjs_is_foreign(context)) {
        nvs_close((nvs_handle)mjs_get_ptr(mjs, context));
    }
}

void thingjsPrefRegister(void) {

    static int thingjs_pref_cases[] = DEF_CASES(DEF_CASE(RES_VIRTUAL));

    static const struct st_thingjs_interface_manifest interface = {
            .type           = INTERFACE_NAME,
            .constructor    = thingjsPrefConstructor,
            .destructor     = thingjsPrefDestructor,
            .cases          = thingjs_pref_cases
    };

    thingjsRegisterInterface(&interface);
}