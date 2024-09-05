/**
 * Copyright Jerily LTD. All Rights Reserved.
 * SPDX-FileCopyrightText: 2024 Neofytos Dimitriou (neo@jerily.cy)
 * SPDX-License-Identifier: MIT.
 */

#include "tjvValidateJson.h"
#include "tjvMessage.h"
#include <cjson/cJSON.h>

// Forward declaration
static void tjv_ValidateJson(const cJSON *json, tjv_ValidationStack *stack, tjv_ValidationElement *ve, Tcl_Obj **error_message_ptr, Tcl_Obj **error_details_ptr, Tcl_Obj **outcome_ptr);

static void tjv_ValidateJsonObject(const cJSON *json, tjv_ValidationStack *stack, tjv_ValidationElement *ve, Tcl_Obj **error_message_ptr, Tcl_Obj **error_details_ptr, Tcl_Obj **outcome_ptr) {

    UNUSED(outcome_ptr);

    DBG2(printf("enter"));

    if (ve->is_nullable && cJSON_IsNull(json)) {
        DBG2(printf("return: ok (null can be accepted)"));
        return;
    }

    // Check if data is valid object
    if (!cJSON_IsObject(json)) {
        tjv_MessageGenerateType(stack, tjv_GetValidationTypeString(ve->type_ex), error_message_ptr, error_details_ptr);
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
                tjv_MessageGenerateRequired(stack, element->key, error_message_ptr, error_details_ptr);
            } else {
                DBG2(printf("check key: [%s] - doesn't exist (OK)", Tcl_GetString(element->key)));
            }
            continue;
        }

        DBG2(printf("check key: [%s]", Tcl_GetString(element->key)));

        // We found a key, let's validate its value.
        tjv_ValidateJson(val, stack, element, error_message_ptr, error_details_ptr, outcome_ptr);

    }

done:

    DBG2(printf("return: ok"));

}

static void tjv_ValidateJsonArray(const cJSON *json, tjv_ValidationStack *stack, tjv_ValidationElement *ve, Tcl_Obj **error_message_ptr, Tcl_Obj **error_details_ptr, Tcl_Obj **outcome_ptr) {

    UNUSED(outcome_ptr);

    DBG2(printf("enter"));

    if (ve->is_nullable && cJSON_IsNull(json)) {
        DBG2(printf("return: ok (null can be accepted)"));
        return;
    }

    if (!cJSON_IsArray(json)) {
        tjv_MessageGenerateType(stack, tjv_GetValidationTypeString(ve->type_ex), error_message_ptr, error_details_ptr);
        DBG2(printf("return: error"));
        return;
    }

    // Do we need to validate list elements?
    if (ve->opts.array_type.element == NULL) {
        goto done;
    }

    // Go throught all keys
    const cJSON *val;
    stack->index = 0;
    cJSON_ArrayForEach(val, json) {
        DBG2(printf("check array element #%" TCL_SIZE_MODIFIER "d", stack->index));
        tjv_ValidateJson(val, stack, ve->opts.array_type.element, error_message_ptr, error_details_ptr, outcome_ptr);
        stack->index++;
    }

done:

    DBG2(printf("return: ok"));

}

static inline void tjv_ValidateJsonInteger(const cJSON *json, tjv_ValidationStack *stack, tjv_ValidationElement *ve, Tcl_Obj **error_message_ptr, Tcl_Obj **error_details_ptr, Tcl_Obj **outcome_ptr) {

    DBG2(printf("enter"));

    if (ve->is_nullable && cJSON_IsNull(json)) {
        DBG2(printf("return: ok (null can be accepted)"));
        return;
    }

    if (!cJSON_IsNumber(json)) {
wrongFormat:
        tjv_MessageGenerateType(stack, tjv_GetValidationTypeString(ve->type_ex), error_message_ptr, error_details_ptr);
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
        tjv_MessageGenerateValue(stack,
            Tcl_ObjPrintf("value is less than the minimum %s", buf),
            error_message_ptr, error_details_ptr);
    } else if (ve->opts.int_type.is_max_value_defined && json->valueint > ve->opts.int_type.max_value) {
        snprintf(buf, sizeof(buf), "%" TCL_LL_MODIFIER "d", ve->opts.int_type.max_value);
        tjv_MessageGenerateValue(stack,
            Tcl_ObjPrintf("value is greater than the maximum %s", buf),
            error_message_ptr, error_details_ptr);
    } else {
        ADD_OUTCOME(Tcl_NewWideIntObj(json->valueint));
    }

    DBG2(printf("return: ok"));

}

static inline void tjv_ValidateJsonDouble(const cJSON *json, tjv_ValidationStack *stack, tjv_ValidationElement *ve, Tcl_Obj **error_message_ptr, Tcl_Obj **error_details_ptr, Tcl_Obj **outcome_ptr) {

    DBG2(printf("enter"));

    if (ve->is_nullable && cJSON_IsNull(json)) {
        DBG2(printf("return: ok (null can be accepted)"));
        return;
    }

    if (!cJSON_IsNumber(json)) {
        tjv_MessageGenerateType(stack, tjv_GetValidationTypeString(ve->type_ex), error_message_ptr, error_details_ptr);
        DBG2(printf("return: error"));
        return;
    }

    double val = cJSON_GetNumberValue(json);

    if (ve->opts.double_type.is_min_value_defined && val < ve->opts.double_type.min_value) {
        tjv_MessageGenerateValue(stack,
            Tcl_ObjPrintf("value is less than the minimum %f", ve->opts.double_type.min_value),
            error_message_ptr, error_details_ptr);
    } else if (ve->opts.double_type.is_max_value_defined && val > ve->opts.double_type.max_value) {
        tjv_MessageGenerateValue(stack,
            Tcl_ObjPrintf("value is greater than the maximum %f", ve->opts.double_type.max_value),
            error_message_ptr, error_details_ptr);
    } else {
        ADD_OUTCOME(Tcl_NewDoubleObj(val));
    }

    DBG2(printf("return: ok"));

}

static inline void tjv_ValidateJsonBoolean(const cJSON *json, tjv_ValidationStack *stack, tjv_ValidationElement *ve, Tcl_Obj **error_message_ptr, Tcl_Obj **error_details_ptr, Tcl_Obj **outcome_ptr) {

    DBG2(printf("enter"));

    if (ve->is_nullable && cJSON_IsNull(json)) {
        DBG2(printf("return: ok (null can be accepted)"));
        return;
    }

    if (!cJSON_IsBool(json)) {
        tjv_MessageGenerateType(stack, tjv_GetValidationTypeString(ve->type_ex), error_message_ptr, error_details_ptr);
        DBG2(printf("return: error"));
        return;
    }

    ADD_OUTCOME(Tcl_NewBooleanObj(cJSON_IsTrue(json) ? 1 : 0));

    DBG2(printf("return: ok"));

}

static inline void tjv_ValidateJsonString(const cJSON *json, tjv_ValidationStack *stack, tjv_ValidationElement *ve, Tcl_Obj **error_message_ptr, Tcl_Obj **error_details_ptr, Tcl_Obj **outcome_ptr) {

    DBG2(printf("enter"));

    if (ve->is_nullable && cJSON_IsNull(json)) {
        DBG2(printf("return: ok (null can be accepted)"));
        return;
    }

    if (!cJSON_IsString(json)) {
        tjv_MessageGenerateType(stack, tjv_GetValidationTypeString(ve->type_ex), error_message_ptr, error_details_ptr);
        DBG2(printf("return: error"));
        return;
    }

    const char *val = cJSON_GetStringValue(json);
    DBG2(printf("string to validate: [%s]", val));

    // If pattern is NULL, we don't need to validate anything
    if (ve->opts.str_type.pattern == NULL) {
        goto done;
    }

    switch (ve->opts.str_type.match) {
    case TJV_STRING_MATCHING_GLOB:

        if (Tcl_StringMatch(val, Tcl_GetString(ve->opts.str_type.pattern)) == 1) {
            goto done;
        }

        tjv_MessageGenerateValue(stack,
            Tcl_ObjPrintf("value does not match the specified glob pattern '%s'", Tcl_GetString(ve->opts.str_type.pattern)),
            error_message_ptr, error_details_ptr);
        goto error;

        break;

    case TJV_STRING_MATCHING_REGEXP: ; // empty statement

        // We have a char* string, but we can't use Tcl_RegExpExec() here because
        // this function doesn't take into account regexp flags. The flags variable
        // there is always 0:
        //      https://github.com/tcltk/tcl/blob/16b5e6486a8e0e1746e35a0a88125ed72e286cd9/generic/tclRegexp.c#L185-L189
        // Also, this function does not provide a significant advantage because it
        // converts the source string from char* to Tcl_DString before matching.
        //
        // So we convert our string into a temporary object to be able to use
        // the modern Tcl_Reg_RExpExecObj() function.

        Tcl_Obj *obj = Tcl_NewStringObj(val, -1);
        int re_result = Tcl_RegExpExecObj(NULL, ve->opts.str_type.regexp, obj, 0, 0, 0);
        Tcl_BounceRefCount(obj);

        if (re_result == 1) {
            goto done;
        }

        if (ve->type_ex == TJV_VALIDATION_EX_STRING) {
            tjv_MessageGenerateValue(stack,
                Tcl_ObjPrintf("value does not match the specified regexp pattern '%s'", Tcl_GetString(ve->opts.str_type.pattern)),
                error_message_ptr, error_details_ptr);
        } else {
            tjv_MessageGenerateType(stack, tjv_GetValidationTypeString(ve->type_ex), error_message_ptr, error_details_ptr);
        }
        goto error;

        break;

    case TJV_STRING_MATCHING_LIST: ; // empty statement

        for (Tcl_Size i = 0; i < ve->opts.str_type.pattern_objc; i++) {
            if (strcmp(val, Tcl_GetString(ve->opts.str_type.pattern_objv[i])) == 0) {
                goto done;
            }
        }

        tjv_MessageGenerateValue(stack,
            Tcl_ObjPrintf("value is not the specified list of allowed values '%s'", Tcl_GetString(ve->opts.str_type.pattern)),
            error_message_ptr, error_details_ptr);
        goto error;

        break;

    }

done:

    ADD_OUTCOME(Tcl_NewStringObj(val, -1));
    DBG2(printf("return: ok"));
    return;

error:

    DBG2(printf("return: error"));
    return;

}

static void tjv_ValidateJson(const cJSON *json, tjv_ValidationStack *stack_parent, tjv_ValidationElement *ve, Tcl_Obj **error_message_ptr, Tcl_Obj **error_details_ptr, Tcl_Obj **outcome_ptr) {

    UNUSED(outcome_ptr);

    DBG2(printf("enter"));

    tjv_ValidationStack stack = { NULL, NULL, (ve->flag == TJV_FLAG_SKIP_KEY ? INT2PTR(1) : ve->key), -1 };
    if (stack_parent == NULL) {
        stack.head = &stack;
    } else {
        stack.head = stack_parent->head;
        stack_parent->next = &stack;
    }

    switch (ve->type) {
    case TJV_VALIDATION_STRING:
        tjv_ValidateJsonString(json, &stack, ve, error_message_ptr, error_details_ptr, outcome_ptr);
        break;
    case TJV_VALIDATION_INTEGER:
        tjv_ValidateJsonInteger(json, &stack, ve, error_message_ptr, error_details_ptr, outcome_ptr);
        break;
    case TJV_VALIDATION_JSON:
        // tjv_ValidateJsonJson(json, &stack, ve, error_message_ptr, error_details_ptr);
        break;
    case TJV_VALIDATION_OBJECT:
        tjv_ValidateJsonObject(json, &stack, ve, error_message_ptr, error_details_ptr, outcome_ptr);
        break;
    case TJV_VALIDATION_ARRAY:
        tjv_ValidateJsonArray(json, &stack, ve, error_message_ptr, error_details_ptr, outcome_ptr);
        break;
    case TJV_VALIDATION_BOOLEAN:
        tjv_ValidateJsonBoolean(json, &stack, ve, error_message_ptr, error_details_ptr, outcome_ptr);
        break;
    case TJV_VALIDATION_DOUBLE:
        tjv_ValidateJsonDouble(json, &stack, ve, error_message_ptr, error_details_ptr, outcome_ptr);
        break;
    }

    if (stack_parent != NULL) {
        stack_parent->next = NULL;
    }

    DBG2(printf("return: ok"));

}

void tjv_ValidateTclJson(Tcl_Obj *data, tjv_ValidationStack *stack, tjv_ValidationElement *ve, Tcl_Obj **error_message_ptr, Tcl_Obj **error_details_ptr, Tcl_Obj **outcome_ptr) {

    DBG2(printf("enter"));

    Tcl_Size length;
    const char *json_string = Tcl_GetStringFromObj(data, &length);
    DBG2(printf("parse json: [%s]", json_string));

    const cJSON *json = cJSON_ParseWithLength(json_string, length);
    if (json == NULL) {
        DBG2(printf("json parse error near: %s", cJSON_GetErrorPtr()));
        tjv_MessageGenerateType(stack, tjv_GetValidationTypeString(ve->type_ex), error_message_ptr, error_details_ptr);
        DBG2(printf("return: error"));
        return;
    }

    switch (ve->flag) {
    case TJV_FLAG_JSON_TYPE_ARRAY:
        DBG2(printf("validate json array"));
        tjv_ValidateJsonArray(json, stack, ve, error_message_ptr, error_details_ptr, outcome_ptr);
        break;
    case TJV_FLAG_JSON_TYPE_OBJECT:
        DBG2(printf("validate json object"));
        tjv_ValidateJsonObject(json, stack, ve, error_message_ptr, error_details_ptr, outcome_ptr);
        break;
    case TJV_FLAG_NONE:
    case TJV_FLAG_SKIP_KEY:
        DBG2(printf("no need to validate json"));
        break;
    }

    cJSON_Delete((cJSON *)json);

    ADD_OUTCOME(data);

    DBG2(printf("return: ok"));

}
