/**
 * Copyright Jerily LTD. All Rights Reserved.
 * SPDX-FileCopyrightText: 2024 Neofytos Dimitriou (neo@jerily.cy)
 * SPDX-License-Identifier: MIT.
 */
#ifndef TJV_VALIDATETCL_H
#define TJV_VALIDATETCL_H

#include "common.h"
#include "tjvCompile.h"

#ifdef __cplusplus
extern "C" {
#endif

void tjv_ValidateTcl(Tcl_Obj *data, Tcl_Size index, tjv_ValidationElement *ve, Tcl_Obj **error_message_ptr, Tcl_Obj **error_details_ptr, Tcl_Obj **outcome_ptr);

#ifdef __cplusplus
}
#endif

#endif // TJV_VALIDATETCL_H
