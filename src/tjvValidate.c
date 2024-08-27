/**
 * Copyright Jerily LTD. All Rights Reserved.
 * SPDX-FileCopyrightText: 2024 Neofytos Dimitriou (neo@jerily.cy)
 * SPDX-License-Identifier: MIT.
 */

#include "tjvValidate.h"


int tjv_ValidateTcl(Tcl_Obj *data, tjv_ValidationElement *ve, char *current_key, Tcl_Obj **error_message_ptr, Tcl_Obj **error_details_ptr) {

    DBG2(printf("enter"));

    int rc = TCL_OK;

    UNUSED(current_key);
    UNUSED(error_message_ptr);
    UNUSED(error_details_ptr);
    UNUSED(data);

    switch (ve->type) {
    case TJV_VALIDATION_STRING:
        //rc = tjv_ValidateTclString(data, ve, error_message_ptr, error_details_ptr);
        break;
    case TJV_VALIDATION_INTEGER:
        //rc = tjv_ValidateTclInteger(data, ve, error_message_ptr, error_details_ptr);
        break;
    case TJV_VALIDATION_JSON:
        // rc = tjv_ValidateJson(data, ve, error_message_ptr, error_details_ptr);
        break;
    case TJV_VALIDATION_OBJECT:
        //rc = tjv_ValidateTclObject(data, ve, error_message_ptr, error_details_ptr);
        break;
    case TJV_VALIDATION_ARRAY:
        //rc = tjv_ValidateTclArray(data, ve, error_message_ptr, error_details_ptr);
        break;
    case TJV_VALIDATION_BOOLEAN:
        //rc = tjv_ValidateTclBoolean(data, ve, error_message_ptr, error_details_ptr);
        break;
    case TJV_VALIDATION_DOUBLE:
        //rc = tjv_ValidateTclDouble(data, ve, error_message_ptr, error_details_ptr);
        break;
    }

    DBG2(printf("return: %s", (rc == TCL_OK ? "ok" : "error")));

}