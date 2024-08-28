/**
 * Copyright Jerily LTD. All Rights Reserved.
 * SPDX-FileCopyrightText: 2024 Neofytos Dimitriou (neo@jerily.cy)
 * SPDX-License-Identifier: MIT.
 */
#ifndef TJV_COMPILE_H
#define TJV_COMPILE_H

#include "common.h"

typedef enum {
    TJV_VALIDATION_OBJECT,
    TJV_VALIDATION_ARRAY,
    TJV_VALIDATION_STRING,
    TJV_VALIDATION_INTEGER,
    TJV_VALIDATION_JSON,
    TJV_VALIDATION_BOOLEAN,
    TJV_VALIDATION_DOUBLE
} tjv_ValidationElementType;

typedef enum {
    TJV_VALIDATION_EX_OBJECT,
    TJV_VALIDATION_EX_ARRAY,
    TJV_VALIDATION_EX_STRING,
    TJV_VALIDATION_EX_INTEGER,
    TJV_VALIDATION_EX_JSON,
    TJV_VALIDATION_EX_BOOLEAN,
    TJV_VALIDATION_EX_DOUBLE,
    TJV_VALIDATION_EX_EMAIL
} tjv_ValidationElementTypeEx;

#define TJV_VALIDATION_TYPE_FROM_EX(x) ( \
    (x) == TJV_VALIDATION_EX_OBJECT  ? TJV_VALIDATION_OBJECT  : \
    (x) == TJV_VALIDATION_EX_ARRAY   ? TJV_VALIDATION_ARRAY   : \
    (x) == TJV_VALIDATION_EX_STRING  ? TJV_VALIDATION_STRING  : \
    (x) == TJV_VALIDATION_EX_INTEGER ? TJV_VALIDATION_INTEGER : \
    (x) == TJV_VALIDATION_EX_JSON    ? TJV_VALIDATION_JSON    : \
    (x) == TJV_VALIDATION_EX_BOOLEAN ? TJV_VALIDATION_BOOLEAN : \
    (x) == TJV_VALIDATION_EX_DOUBLE  ? TJV_VALIDATION_DOUBLE  : \
    (x) == TJV_VALIDATION_EX_EMAIL   ? TJV_VALIDATION_STRING  : \
    -1 )

#define TJV_VALIDATION_EX_TYPE_STR(x) ( \
    (x) == TJV_VALIDATION_EX_OBJECT  ? "TJV_VALIDATION_EX_OBJECT"  : \
    (x) == TJV_VALIDATION_EX_ARRAY   ? "TJV_VALIDATION_EX_ARRAY"   : \
    (x) == TJV_VALIDATION_EX_STRING  ? "TJV_VALIDATION_EX_STRING"  : \
    (x) == TJV_VALIDATION_EX_INTEGER ? "TJV_VALIDATION_EX_INTEGER" : \
    (x) == TJV_VALIDATION_EX_JSON    ? "TJV_VALIDATION_EX_JSON"    : \
    (x) == TJV_VALIDATION_EX_BOOLEAN ? "TJV_VALIDATION_EX_BOOLEAN" : \
    (x) == TJV_VALIDATION_EX_DOUBLE  ? "TJV_VALIDATION_EX_DOUBLE"  : \
    (x) == TJV_VALIDATION_EX_EMAIL   ? "TJV_VALIDATION_EX_EMAIL"   : \
    "ERROR - UNKNOWN VALIDATION TYPE" )

typedef enum {
    TJV_STRING_MATCHING_GLOB,
    TJV_STRING_MATCHING_REGEXP,
    TJV_STRING_MATCHING_LIST
} tjv_ValidationStringMatchingType;

typedef enum {
    TJV_JSON_TYPE_NONE,
    TJV_JSON_TYPE_OBJECT,
    TJV_JSON_TYPE_ARRAY
} tjv_ValidationJsonType;

typedef struct tjv_ValidationElement tjv_ValidationElement;

struct tjv_ValidationElement {
    // Common options
    tjv_ValidationElementType type;
    tjv_ValidationElementTypeEx type_ex;
    int is_required;
    int is_nullable;
    Tcl_Obj *command;
    Tcl_Obj *key;
    Tcl_Obj *path;
    Tcl_Obj *outkey;

    // Type-specific options
    tjv_ValidationJsonType json_type;
    union {
        // options for TJV_VALIDATION_STRING
        struct {
            tjv_ValidationStringMatchingType match;
            Tcl_Obj *pattern;
            Tcl_RegExp regexp;
            // cache for faster access
            Tcl_Size pattern_objc;
            Tcl_Obj **pattern_objv;
        } str_type;
        // options for TJV_VALIDATION_INTEGER
        struct {
            Tcl_WideInt min_value;
            int is_min_value_defined;
            Tcl_WideInt max_value;
            int is_max_value_defined;
        } int_type;
        // options for TJV_VALIDATION_OBJECT and TJV_VALIDATION_JSON
        struct {
            Tcl_Obj *keys_list;
            tjv_ValidationElement **elements;
            // cache for faster access
            Tcl_Size keys_objc;
            Tcl_Obj **keys_objv;
        } obj_type;
        // options for TJV_VALIDATION_DOUBLE
        struct {
            double min_value;
            int is_min_value_defined;
            double max_value;
            int is_max_value_defined;
        } double_type;
        // options for TJV_VALIDATION_ARRAY and TJV_VALIDATION_JSON
        struct {
            tjv_ValidationElement *element;
        } array_type;
    } opts;

};


#ifdef __cplusplus
extern "C" {
#endif

void tjv_ValidationCompileInit(void);
void tjv_ValidationElementFree(tjv_ValidationElement *ve);
tjv_ValidationElement *tjv_ValidationCompile(Tcl_Interp *interp, Tcl_Size objc, Tcl_Obj *const objv[], Tcl_Obj *path, Tcl_Obj **last_arg);

#ifdef __cplusplus
}
#endif

#endif // TJV_COMPILE_H
