/**
 * Copyright Jerily LTD. All Rights Reserved.
 * SPDX-FileCopyrightText: 2024 Neofytos Dimitriou (neo@jerily.cy)
 * SPDX-License-Identifier: MIT.
 */

#include "tjvValidateJson.h"
#include "tjvMessage.h"
#include <cjson/cJSON.h>

// Forward declaration
static void tjv_ValidateJson(const cJSON *json, Tcl_Size index, tjv_ValidationElement *ve, Tcl_Obj **error_message_ptr, Tcl_Obj **error_details_ptr);

static void tjv_ValidateJsonObject(const cJSON *json, Tcl_Size index, tjv_ValidationElement *ve, Tcl_Obj **error_message_ptr, Tcl_Obj **error_details_ptr) {

    DBG2(printf("enter"));

    if (ve->is_nullable && cJSON_IsNull(json)) {
        DBG2(printf("return: ok (null can be accepted)"));
        return;
    }

    // Check if data is valid object
    if (!cJSON_IsObject(json)) {
        tjv_MessageGenerateType(ve->path, index, "object", error_message_ptr, error_details_ptr);
        DBG2(printf("return: error"));
        return;
    }

    // Do we have keys to validate?
    if (ve->opts.obj_type.keys_list == NULL) {
        goto done;
    }

    // Go throught all keys
    for (Tcl_Size i = 0; i < ve->opts.obj_type.keys_objc; i++) {

        tjv_ValidationElement *element = ve->opts.obj_type.elements[i];

        cJSON *val = cJSON_GetObjectItem(json, Tcl_GetString(element->key));
        if (val == NULL) {
            // There is no such key. Report an error if it is required.
            if (element->is_required) {
                DBG2(printf("check key: [%s] - doesn't exist (ERROR)", Tcl_GetString(element->key)));
                tjv_MessageGenerateRequired(ve->path, index, element->key, error_message_ptr, error_details_ptr);
            } else {
                DBG2(printf("check key: [%s] - doesn't exist (OK)", Tcl_GetString(element->key)));
            }
            continue;
        }

        DBG2(printf("check key: [%s]", Tcl_GetString(element->key)));

        // We found a key, let's validate its value.
        tjv_ValidateJson(val, -1, element, error_message_ptr, error_details_ptr);

    }

done:

    DBG2(printf("return: ok"));

}

static void tjv_ValidateJsonArray(const cJSON *json, Tcl_Size index, tjv_ValidationElement *ve, Tcl_Obj **error_message_ptr, Tcl_Obj **error_details_ptr) {

    DBG2(printf("enter"));

    if (ve->is_nullable && cJSON_IsNull(json)) {
        DBG2(printf("return: ok (null can be accepted)"));
        return;
    }

    if (!cJSON_IsArray(json)) {
        tjv_MessageGenerateType(ve->path, index, "array", error_message_ptr, error_details_ptr);
        DBG2(printf("return: error"));
        return;
    }

    // Do we need to validate list elements?
    if (ve->opts.array_type.element == NULL) {
        goto done;
    }

    // Go throught all keys
    const cJSON *val;
    Tcl_Size i = 0;
    cJSON_ArrayForEach(val, json) {
        DBG2(printf("check array element #%" TCL_SIZE_MODIFIER "d", i));
        tjv_ValidateJson(val, i, ve->opts.array_type.element, error_message_ptr, error_details_ptr);
        i++;
    }

done:

    DBG2(printf("return: ok"));

}

static void tjv_ValidateJsonInteger(const cJSON *json, Tcl_Size index, tjv_ValidationElement *ve, Tcl_Obj **error_message_ptr, Tcl_Obj **error_details_ptr) {

    DBG2(printf("enter"));

    if (ve->is_nullable && cJSON_IsNull(json)) {
        DBG2(printf("return: ok (null can be accepted)"));
        return;
    }

    if (!cJSON_IsNumber(json)) {
wrongFormat:
        tjv_MessageGenerateType(ve->path, index, "integer", error_message_ptr, error_details_ptr);
        DBG2(printf("return: error"));
        return;
    }

    // Make sure that the value is an integer
    double val = cJSON_GetNumberValue(json);
    if (val != (double)json->valueint) {
        goto wrongFormat;
    }

    char buf[64];

    if (ve->opts.int_type.is_min_value_defined && json->valueint < ve->opts.int_type.min_value) {
        snprintf(buf, sizeof(buf), "%" TCL_LL_MODIFIER "d", ve->opts.int_type.min_value);
        tjv_MessageGenerateValue(ve->path, index,
            Tcl_ObjPrintf("value is less than the minimum %s", buf),
            error_message_ptr, error_details_ptr);
    }

    if (ve->opts.int_type.is_max_value_defined && json->valueint > ve->opts.int_type.max_value) {
        snprintf(buf, sizeof(buf), "%" TCL_LL_MODIFIER "d", ve->opts.int_type.max_value);
        tjv_MessageGenerateValue(ve->path, index,
            Tcl_ObjPrintf("value is greater than the maximum %s", buf),
            error_message_ptr, error_details_ptr);
    }

    DBG2(printf("return: ok"));

}

static void tjv_ValidateJsonDouble(const cJSON *json, Tcl_Size index, tjv_ValidationElement *ve, Tcl_Obj **error_message_ptr, Tcl_Obj **error_details_ptr) {

    DBG2(printf("enter"));

    if (ve->is_nullable && cJSON_IsNull(json)) {
        DBG2(printf("return: ok (null can be accepted)"));
        return;
    }

    if (!cJSON_IsNumber(json)) {
        tjv_MessageGenerateType(ve->path, index, "double", error_message_ptr, error_details_ptr);
        DBG2(printf("return: error"));
        return;
    }

    double val = cJSON_GetNumberValue(json);

    if (ve->opts.double_type.is_min_value_defined && val < ve->opts.double_type.min_value) {
        tjv_MessageGenerateValue(ve->path, index,
            Tcl_ObjPrintf("value is less than the minimum %f", ve->opts.double_type.min_value),
            error_message_ptr, error_details_ptr);
    }

    if (ve->opts.double_type.is_max_value_defined && val > ve->opts.double_type.max_value) {
        tjv_MessageGenerateValue(ve->path, index,
            Tcl_ObjPrintf("value is greater than the maximum %f", ve->opts.double_type.max_value),
            error_message_ptr, error_details_ptr);
    }

    DBG2(printf("return: ok"));

}

static void tjv_ValidateJsonBoolean(const cJSON *json, Tcl_Size index, tjv_ValidationElement *ve, Tcl_Obj **error_message_ptr, Tcl_Obj **error_details_ptr) {

    DBG2(printf("enter"));

    if (ve->is_nullable && cJSON_IsNull(json)) {
        DBG2(printf("return: ok (null can be accepted)"));
        return;
    }

    if (!cJSON_IsBool(json)) {
        tjv_MessageGenerateType(ve->path, index, "boolean", error_message_ptr, error_details_ptr);
        DBG2(printf("return: error"));
        return;
    }

    DBG2(printf("return: ok"));

}

static void tjv_ValidateJsonString(const cJSON *json, Tcl_Size index, tjv_ValidationElement *ve, Tcl_Obj **error_message_ptr, Tcl_Obj **error_details_ptr) {

    DBG2(printf("enter"));

    if (ve->is_nullable && cJSON_IsNull(json)) {
        DBG2(printf("return: ok (null can be accepted)"));
        return;
    }

    if (!cJSON_IsString(json)) {
        tjv_MessageGenerateType(ve->path, index, "string", error_message_ptr, error_details_ptr);
        DBG2(printf("return: error"));
        return;
    }

    // If pattern is NULL, we don't need to validate anything
    if (ve->opts.str_type.pattern == NULL) {
        goto done;
    }

    const char *val = cJSON_GetStringValue(json);

    switch (ve->opts.str_type.match) {
    case TJV_STRING_MATCHING_GLOB:

        if (Tcl_StringMatch(val, Tcl_GetString(ve->opts.str_type.pattern)) != 1) {
            tjv_MessageGenerateValue(ve->path, index,
                Tcl_ObjPrintf("value does not match the specified glob pattern '%s'", Tcl_GetString(ve->opts.str_type.pattern)),
                error_message_ptr, error_details_ptr);
        }

        break;
    case TJV_STRING_MATCHING_REGEXP: ; // empty statement

        if (Tcl_RegExpExec(NULL, ve->opts.str_type.regexp, val, 0) != 1) {
            tjv_MessageGenerateValue(ve->path, index,
                Tcl_ObjPrintf("value does not match the specified regexp pattern '%s'", Tcl_GetString(ve->opts.str_type.pattern)),
                error_message_ptr, error_details_ptr);
        }

        break;
    case TJV_STRING_MATCHING_LIST: ; // empty statement

        int found = 0;
        for (Tcl_Size i = 0; i < ve->opts.str_type.pattern_objc; i++) {
            if (strcmp(val, Tcl_GetString(ve->opts.str_type.pattern_objv[i])) == 0) {
                found = 1;
                break;
            }
        }

        if (!found) {
            tjv_MessageGenerateValue(ve->path, index,
                Tcl_ObjPrintf("value is not the specified list of allowed values '%s'", Tcl_GetString(ve->opts.str_type.pattern)),
                error_message_ptr, error_details_ptr);
        }

        break;
    }

done:

    DBG2(printf("return: ok"));

}

static void tjv_ValidateJson(const cJSON *json, Tcl_Size index, tjv_ValidationElement *ve, Tcl_Obj **error_message_ptr, Tcl_Obj **error_details_ptr) {

    DBG2(printf("enter"));

    switch (ve->type) {
    case TJV_VALIDATION_STRING:
        tjv_ValidateJsonString(json, index, ve, error_message_ptr, error_details_ptr);
        break;
    case TJV_VALIDATION_INTEGER:
        tjv_ValidateJsonInteger(json, index, ve, error_message_ptr, error_details_ptr);
        break;
    case TJV_VALIDATION_JSON:
        // tjv_ValidateJsonJson(json, index, ve, error_message_ptr, error_details_ptr);
        break;
    case TJV_VALIDATION_OBJECT:
        tjv_ValidateJsonObject(json, index, ve, error_message_ptr, error_details_ptr);
        break;
    case TJV_VALIDATION_ARRAY:
        tjv_ValidateJsonArray(json, index, ve, error_message_ptr, error_details_ptr);
        break;
    case TJV_VALIDATION_BOOLEAN:
        tjv_ValidateJsonBoolean(json, index, ve, error_message_ptr, error_details_ptr);
        break;
    case TJV_VALIDATION_DOUBLE:
        tjv_ValidateJsonDouble(json, index, ve, error_message_ptr, error_details_ptr);
        break;
    }

    DBG2(printf("return: ok"));

}

void tjv_ValidateTclJson(Tcl_Obj *data, Tcl_Size index, tjv_ValidationElement *ve, Tcl_Obj **error_message_ptr, Tcl_Obj **error_details_ptr) {

    DBG2(printf("enter"));

    DBG2(printf("parse json"));
    Tcl_Size length;
    const char *json_string = Tcl_GetStringFromObj(data, &length);

    const cJSON *json = cJSON_ParseWithLength(json_string, length);
    if (json == NULL) {
        DBG2(printf("return: error"));
        return;
    }

    if (ve->json_type == TJV_JSON_TYPE_ARRAY) {
        DBG2(printf("validate json array"));
        tjv_ValidateJsonArray(json, index, ve, error_message_ptr, error_details_ptr);
    } else if (ve->json_type == TJV_JSON_TYPE_OBJECT) {
        DBG2(printf("validate json object"));
        tjv_ValidateJsonObject(json, index, ve, error_message_ptr, error_details_ptr);
    } else {
        DBG2(printf("no need to validate json"));
    }

    cJSON_Delete((cJSON *)json);

    DBG2(printf("return: ok"));

}
