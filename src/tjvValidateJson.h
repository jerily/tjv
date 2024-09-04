/**
 * Copyright Jerily LTD. All Rights Reserved.
 * SPDX-FileCopyrightText: 2024 Neofytos Dimitriou (neo@jerily.cy)
 * SPDX-License-Identifier: MIT.
 */
#ifndef TJV_VALIDATEJSON_H
#define TJV_VALIDATEJSON_H

#include "common.h"
#include "tjvCompile.h"

#ifdef __cplusplus
extern "C" {
#endif

void tjv_ValidateTclJson(Tcl_Obj *data, tjv_ValidationStack *stack_parent, tjv_ValidationElement *ve, Tcl_Obj **error_message_ptr, Tcl_Obj **error_details_ptr, Tcl_Obj **outcome_ptr);

#ifdef __cplusplus
}
#endif

#endif // TJV_VALIDATEJSON_H
