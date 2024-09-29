/**
 * Copyright Jerily LTD. All Rights Reserved.
 * SPDX-FileCopyrightText: 2024 Neofytos Dimitriou (neo@jerily.cy)
 * SPDX-License-Identifier: MIT.
 */
#ifndef TJV_MESSAGE_H
#define TJV_MESSAGE_H

#include "common.h"

typedef enum {
    TJV_MSG_KEYWORD_TYPE,
    TJV_MSG_KEYWORD_REQUIRED,
    TJV_MSG_KEYWORD_VALUE
} tjv_MessageErrorKeywordType;

#ifdef __cplusplus
extern "C" {
#endif

void tjv_MessageInit(void);

Tcl_Obj *tjv_MessageCombineDetails(Tcl_Obj *error_message, Tcl_Obj *error_details);
Tcl_Obj *tjv_MessageCombine(Tcl_Obj *error_message);

void tjv_MessageGenerate(tjv_MessageErrorKeywordType keyword_type, tjv_ValidationStack *stack, Tcl_Obj *message,
    Tcl_Obj **error_message_ptr, Tcl_Obj **error_details_ptr);

void tjv_MessageGenerateRequired(tjv_ValidationStack *stack, Tcl_Obj *required_key,
    Tcl_Obj **error_message_ptr, Tcl_Obj **error_details_ptr);

void tjv_MessageGenerateType(tjv_ValidationStack *stack, const char *required_type,
    Tcl_Obj **error_message_ptr, Tcl_Obj **error_details_ptr);

void tjv_MessageGenerateValue(tjv_ValidationStack *stack, Tcl_Obj *message,
    Tcl_Obj **error_message_ptr, Tcl_Obj **error_details_ptr);

#ifdef __cplusplus
}
#endif

#endif // TJV_MESSAGE_H
