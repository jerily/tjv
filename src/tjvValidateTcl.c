/**
 * Copyright Jerily LTD. All Rights Reserved.
 * SPDX-FileCopyrightText: 2024 Neofytos Dimitriou (neo@jerily.cy)
 * SPDX-License-Identifier: MIT.
 */

#include "tjvValidateTcl.h"
#include "tjvValidateJson.h"
#include "tjvMessage.h"

static void tjv_ValidateTclObject(Tcl_Obj *data, Tcl_Size index, tjv_ValidationElement *ve, Tcl_Obj **error_message_ptr, Tcl_Obj **error_details_ptr) {

    DBG2(printf("enter"));

    // Check if data is valid dict
    int size;
    if (Tcl_DictObjSize(NULL, data, &size) != TCL_OK) {
        tjv_MessageGenerateType(ve->path, index, "object (Tcl dict)", error_message_ptr, error_details_ptr);
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

        // Try to get a key
        Tcl_Obj *val;
        if (Tcl_DictObjGet(NULL, data, element->key, &val) != TCL_OK) {
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
        tjv_ValidateTcl(val, -1, element, error_message_ptr, error_details_ptr);

    }

done:

    DBG2(printf("return: ok"));

}

static void tjv_ValidateTclArray(Tcl_Obj *data, Tcl_Size index, tjv_ValidationElement *ve, Tcl_Obj **error_message_ptr, Tcl_Obj **error_details_ptr) {

    DBG2(printf("enter"));

    // Check if data is valid list
    Tcl_Size items_objc;
    Tcl_Obj **items_objv;
    if (Tcl_ListObjGetElements(NULL, data, &items_objc, &items_objv) != TCL_OK) {
        tjv_MessageGenerateType(ve->path, index, "array (Tcl list)", error_message_ptr, error_details_ptr);
        DBG2(printf("return: error"));
        return;
    }

    // Do we need to validate list elements?
    if (ve->opts.array_type.element == NULL) {
        goto done;
    }

    // Go throught all keys
    for (Tcl_Size i = 0; i < items_objc; i++) {

        DBG2(printf("check array element #%" TCL_SIZE_MODIFIER "d", i));

        tjv_ValidateTcl(items_objv[i], i, ve->opts.array_type.element, error_message_ptr, error_details_ptr);

    }

done:

    DBG2(printf("return: ok"));

}

static void tjv_ValidateTclInteger(Tcl_Obj *data, Tcl_Size index, tjv_ValidationElement *ve, Tcl_Obj **error_message_ptr, Tcl_Obj **error_details_ptr) {

    DBG2(printf("enter"));

    Tcl_WideInt val;
    if (Tcl_GetWideIntFromObj(NULL, data, &val) != TCL_OK) {
        tjv_MessageGenerateType(ve->path, index, tjv_GetValidationTypeString(ve->type_ex), error_message_ptr, error_details_ptr);
        DBG2(printf("return: error"));
        return;
    }

    char buf[64];

    if (ve->opts.int_type.is_min_value_defined && val < ve->opts.int_type.min_value) {
        snprintf(buf, sizeof(buf), "%" TCL_LL_MODIFIER "d", ve->opts.int_type.min_value);
        tjv_MessageGenerateValue(ve->path, index,
            Tcl_ObjPrintf("value is less than the minimum %s", buf),
            error_message_ptr, error_details_ptr);
    }

    if (ve->opts.int_type.is_max_value_defined && val > ve->opts.int_type.max_value) {
        snprintf(buf, sizeof(buf), "%" TCL_LL_MODIFIER "d", ve->opts.int_type.max_value);
        tjv_MessageGenerateValue(ve->path, index,
            Tcl_ObjPrintf("value is greater than the maximum %s", buf),
            error_message_ptr, error_details_ptr);
    }

    DBG2(printf("return: ok"));

}

static void tjv_ValidateTclDouble(Tcl_Obj *data, Tcl_Size index, tjv_ValidationElement *ve, Tcl_Obj **error_message_ptr, Tcl_Obj **error_details_ptr) {

    DBG2(printf("enter"));

    double val;
    if (Tcl_GetDoubleFromObj(NULL, data, &val) != TCL_OK) {
        tjv_MessageGenerateType(ve->path, index, tjv_GetValidationTypeString(ve->type_ex), error_message_ptr, error_details_ptr);
        DBG2(printf("return: error"));
        return;
    }

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

static void tjv_ValidateTclBoolean(Tcl_Obj *data, Tcl_Size index, tjv_ValidationElement *ve, Tcl_Obj **error_message_ptr, Tcl_Obj **error_details_ptr) {

    DBG2(printf("enter"));

    int val;
    if (Tcl_GetBooleanFromObj(NULL, data, &val) != TCL_OK) {
        tjv_MessageGenerateType(ve->path, index, tjv_GetValidationTypeString(ve->type_ex), error_message_ptr, error_details_ptr);
        DBG2(printf("return: error"));
        return;
    }

    DBG2(printf("return: ok"));

}

static void tjv_ValidateTclString(Tcl_Obj *data, Tcl_Size index, tjv_ValidationElement *ve, Tcl_Obj **error_message_ptr, Tcl_Obj **error_details_ptr) {

    DBG2(printf("enter"));

    // If pattern is NULL, we don't need to validate anything
    if (ve->opts.str_type.pattern == NULL) {
        goto done;
    }

    switch (ve->opts.str_type.match) {
    case TJV_STRING_MATCHING_GLOB:

        if (Tcl_StringMatch(Tcl_GetString(data), Tcl_GetString(ve->opts.str_type.pattern)) != 1) {
            tjv_MessageGenerateValue(ve->path, index,
                Tcl_ObjPrintf("value does not match the specified glob pattern '%s'", Tcl_GetString(ve->opts.str_type.pattern)),
                error_message_ptr, error_details_ptr);
        }

        break;
    case TJV_STRING_MATCHING_REGEXP:

        if (Tcl_RegExpExecObj(NULL, ve->opts.str_type.regexp, data, 0, 0, 0) != 1) {
            if (ve->type_ex == TJV_VALIDATION_EX_STRING) {
                tjv_MessageGenerateValue(ve->path, index,
                    Tcl_ObjPrintf("value does not match the specified regexp pattern '%s'", Tcl_GetString(ve->opts.str_type.pattern)),
                    error_message_ptr, error_details_ptr);
            } else {
                tjv_MessageGenerateType(ve->path, index, tjv_GetValidationTypeString(ve->type_ex), error_message_ptr, error_details_ptr);
            }
        }

        break;
    case TJV_STRING_MATCHING_LIST: ; // empty statement

        int found = 0;
        const char *str = Tcl_GetString(data);
        for (Tcl_Size i = 0; i < ve->opts.str_type.pattern_objc; i++) {
            if (strcmp(str, Tcl_GetString(ve->opts.str_type.pattern_objv[i])) == 0) {
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


void tjv_ValidateTcl(Tcl_Obj *data, Tcl_Size index, tjv_ValidationElement *ve, Tcl_Obj **error_message_ptr, Tcl_Obj **error_details_ptr) {

    DBG2(printf("enter"));

    switch (ve->type) {
    case TJV_VALIDATION_STRING:
        tjv_ValidateTclString(data, index, ve, error_message_ptr, error_details_ptr);
        break;
    case TJV_VALIDATION_INTEGER:
        tjv_ValidateTclInteger(data, index, ve, error_message_ptr, error_details_ptr);
        break;
    case TJV_VALIDATION_JSON:
        tjv_ValidateTclJson(data, index, ve, error_message_ptr, error_details_ptr);
        break;
    case TJV_VALIDATION_OBJECT:
        tjv_ValidateTclObject(data, index, ve, error_message_ptr, error_details_ptr);
        break;
    case TJV_VALIDATION_ARRAY:
        tjv_ValidateTclArray(data, index, ve, error_message_ptr, error_details_ptr);
        break;
    case TJV_VALIDATION_BOOLEAN:
        tjv_ValidateTclBoolean(data, index, ve, error_message_ptr, error_details_ptr);
        break;
    case TJV_VALIDATION_DOUBLE:
        tjv_ValidateTclDouble(data, index, ve, error_message_ptr, error_details_ptr);
        break;
    }

    DBG2(printf("return: %s", (*error_message_ptr == NULL ? "ok" : "error")));

}