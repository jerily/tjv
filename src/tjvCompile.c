/**
 * Copyright Jerily LTD. All Rights Reserved.
 * SPDX-FileCopyrightText: 2024 Neofytos Dimitriou (neo@jerily.cy)
 * SPDX-License-Identifier: MIT.
 */

#include "tjvCompile.h"

#define TJV_REGEXP_EMAIL "^[A-Za-z0-9._-]+@[[A-Za-z0-9.-]+$"

typedef struct ThreadSpecificData {
    Tcl_Obj *email_pattern;
    Tcl_RegExp email_regexp;
} ThreadSpecificData;

static Tcl_ThreadDataKey dataKey;

#define TCL_TSD_INIT(keyPtr) \
    (ThreadSpecificData *)Tcl_GetThreadData((keyPtr), sizeof(ThreadSpecificData))

static int tjv_ValidationCompile_initialized = 0;
static Tcl_Mutex tjv_ValidationCompile_initialize_mx;

// Forward declarations
tjv_ValidationElement *tjv_ValidationCompile(Tcl_Interp *interp, Tcl_Size objc, Tcl_Obj *const objv[], Tcl_Obj **last_arg);

static tjv_ValidationElement *tjv_ValidationElementAlloc(tjv_ValidationElementTypeEx type) {

    tjv_ValidationElement *rc = ckalloc(sizeof(tjv_ValidationElement));
    memset(rc, 0, sizeof(tjv_ValidationElement));

    rc->type_ex = type;
    rc->type = TJV_VALIDATION_TYPE_FROM_EX(type);
    assert(rc->type != (unsigned)-1 && "unable to convert tjv_ValidationElementTypeEx to tjv_ValidationElementType");

    return rc;

}

void tjv_ValidationElementFree(tjv_ValidationElement *ve) {

    DBG2(printf("enter: ve: %p", (void *)ve));

    if (ve->command != NULL) {
        Tcl_DecrRefCount(ve->command);
    }

    switch (ve->type) {
    case TJV_VALIDATION_STRING:
        if (ve->opts.str_type.pattern != NULL) {
            Tcl_DecrRefCount(ve->opts.str_type.pattern);
        }
        break;
    case TJV_VALIDATION_INTEGER:
        break;
    case TJV_VALIDATION_JSON:
        break;
    case TJV_VALIDATION_OBJECT:
        if (ve->opts.obj_type.elements != NULL) {
            for (Tcl_Size i = 0; ve->opts.obj_type.elements[i] != NULL; i++) {
                tjv_ValidationElementFree(ve->opts.obj_type.elements[i]);
            }
            ckfree(ve->opts.obj_type.elements);
        }
        if (ve->opts.obj_type.keys_list != NULL) {
            Tcl_DecrRefCount(ve->opts.obj_type.keys_list);
        }
        break;
    case TJV_VALIDATION_ARRAY:
        if (ve->opts.array_type.element != NULL) {
            tjv_ValidationElementFree(ve->opts.array_type.element);
        }
        break;
    case TJV_VALIDATION_BOOLEAN:
        break;
    case TJV_VALIDATION_DOUBLE:
        break;
    }

    ckfree(ve);

    DBG2(printf("return: ok"));

}

static int tjv_ValidationCompileItems(Tcl_Interp *interp, Tcl_Obj *data, tjv_ValidationElement **element) {

    DBG2(printf("enter"));

    // We will use tjv_ValidationCompile() to parse items format and this
    // function uses Tcl_ParseArgsObjv() to parse parameters.
    // Tcl_ParseArgsObjv() expects that the first parameter of passed objv
    // to be a function name. But in this case, the options start from
    // the first parameter. To successfully parse our parameters, we need
    // to add a stub as the first parameter. Thus, we have to work with
    // a duplicated object.

    Tcl_Obj *items_format = Tcl_DuplicateObj(data);
    Tcl_Obj *stub_parameter = Tcl_NewStringObj("items", -1);
    if (Tcl_ListObjReplace(interp, items_format, 0, 0, 1, &stub_parameter) != TCL_OK) {
        DBG2(printf("return: error (wrong list format)"));
        Tcl_BounceRefCount(stub_parameter);
        Tcl_BounceRefCount(items_format);
        return TCL_ERROR;
    }

    Tcl_Size items_objc;
    Tcl_Obj **items_objv;
    // We have already successfully inserted a stub parameter into items_format.
    // Therefore, we are confident that this is the correct list and do not check
    // for errors here.
    Tcl_ListObjGetElements(NULL, items_format, &items_objc, &items_objv);

    *element = tjv_ValidationCompile(interp, items_objc, items_objv, NULL);
    Tcl_BounceRefCount(items_format);

    if (*element == NULL) {
        DBG2(printf("return: error (failed to items format)"));
        return TCL_ERROR;
    }


    DBG2(printf("return: ok"));
    return TCL_OK;

}

static int tjv_ValidationCompileProperties(Tcl_Interp *interp, Tcl_Obj *data, Tcl_Obj **keys_list_ptr, tjv_ValidationElement ***elements_ptr) {

    DBG2(printf("enter"));

    Tcl_Size child_count;
    if (Tcl_ListObjLength(interp, data, &child_count) != TCL_OK) {
        return TCL_ERROR;
    }

    DBG2(printf("child properties count: %" TCL_SIZE_MODIFIER "d", child_count));

    if (child_count == 0) {
        DBG2(printf("return: ok (no keys)"));
        return TCL_OK;
    }

    tjv_ValidationElement **elements = ckalloc(sizeof(tjv_ValidationElement*) * (child_count + 1));
    memset(elements, 0, (sizeof(tjv_ValidationElement*) * (child_count + 1)));
    *elements_ptr = elements;


    Tcl_Obj *keys_list = Tcl_NewListObj(0, NULL);
    Tcl_IncrRefCount(keys_list);
    *keys_list_ptr = keys_list;

    for (Tcl_Size i = 0; i < child_count; i++) {

        Tcl_Obj *child_element;
        Tcl_ListObjIndex(NULL, data, i, &child_element);

        Tcl_Size child_objc;
        Tcl_Obj **child_objv;
        if (Tcl_ListObjGetElements(NULL, child_element, &child_objc, &child_objv) != TCL_OK) {
            Tcl_SetObjResult(interp, Tcl_ObjPrintf("object element #%" TCL_SIZE_MODIFIER "d"
                " is malformed", i));
            DBG2(printf("return: error (element #%" TCL_SIZE_MODIFIER "d is not a list", i));
            return TCL_ERROR;
        }

        if (child_objc < 1) {
            Tcl_SetObjResult(interp, Tcl_ObjPrintf("object element #%" TCL_SIZE_MODIFIER "d"
                " is an empty list", i));
            DBG2(printf("return: error (element #%" TCL_SIZE_MODIFIER "d is an empty list", i));
            return TCL_ERROR;
        }

        Tcl_ListObjAppendElement(NULL, keys_list, child_objv[0]);
        const char *key_name = Tcl_GetString(child_objv[0]);

        DBG2(printf("parse property: [%s]", key_name));
        elements[i] = tjv_ValidationCompile(interp, child_objc, child_objv, NULL);
        if (elements[i] == NULL) {

            DBG2(printf("return: error (failed to parse element #%" TCL_SIZE_MODIFIER "d [%s]: %s",
                i, key_name, Tcl_GetStringResult(interp)));

            Tcl_SetObjResult(interp, Tcl_ObjPrintf("%s->%s", key_name, Tcl_GetStringResult(interp)));
            return TCL_ERROR;

        }

    }

    DBG2(printf("return: ok"));
    return TCL_OK;

}

static int copy_arg(void *clientData, Tcl_Obj *objPtr, void *dstPtr) {
    UNUSED(clientData);
    if (objPtr == NULL) {
        *((void **)dstPtr) = INT2PTR(1);
    } else {
        *((Tcl_Obj **)dstPtr) = objPtr;
    }
    return 1;
}

tjv_ValidationElement *tjv_ValidationCompile(Tcl_Interp *interp, Tcl_Size objc, Tcl_Obj *const objv[], Tcl_Obj **last_arg) {

    DBG2(printf("enter: objc: %d", objc));

    int idx;
    tjv_ValidationElement *rc = NULL;
    Tcl_Obj **remObjv = NULL;

    Tcl_Obj *opt_type = NULL;
    int opt_is_required = 0;
    int opt_is_nullable = 0;
    Tcl_Obj *opt_command = NULL;
    Tcl_Obj *opt_match = NULL;
    Tcl_Obj *opt_pattern = NULL;
    Tcl_Obj *opt_minimum = NULL;
    Tcl_Obj *opt_maximum = NULL;
    Tcl_Obj *opt_properties = NULL;
    Tcl_Obj *opt_items = NULL;

#pragma GCC diagnostic push
// ignore warning for copy_arg:
//     warning: ISO C forbids conversion of function pointer to object pointer type [-Wpedantic]
#pragma GCC diagnostic ignored "-Wpedantic"
    Tcl_ArgvInfo ArgTable[] = {
        { TCL_ARGV_FUNC,     "-type",       copy_arg,   &opt_type,        NULL, NULL },
        { TCL_ARGV_CONSTANT, "-required",   INT2PTR(1), &opt_is_required, NULL, NULL },
        { TCL_ARGV_CONSTANT, "-nullable",   INT2PTR(1), &opt_is_nullable, NULL, NULL },
        { TCL_ARGV_FUNC,     "-command",    copy_arg,   &opt_command,     NULL, NULL },
        // TJV_VALIDATION_STRING
        { TCL_ARGV_FUNC,     "-match",      copy_arg,   &opt_match,       NULL, NULL },
        { TCL_ARGV_FUNC,     "-pattern",    copy_arg,   &opt_pattern,     NULL, NULL },
        // TJV_VALIDATION_INTEGER / TJV_VALIDATION_DOUBLE
        { TCL_ARGV_FUNC,     "-minimum",    copy_arg,   &opt_minimum,     NULL, NULL },
        { TCL_ARGV_FUNC,     "-maximum",    copy_arg,   &opt_maximum,     NULL, NULL },
        // TJV_VALIDATION_OBJECT
        { TCL_ARGV_FUNC,     "-properties", copy_arg,   &opt_properties,  NULL, NULL },
        // TJV_VALIDATION_ARRAY
        { TCL_ARGV_FUNC,     "-items",      copy_arg,   &opt_items,       NULL, NULL },
        TCL_ARGV_TABLE_END
    };
#pragma GCC diagnostic pop

    static const struct {
        const char *type_name;
        tjv_ValidationElementTypeEx type;
    } type_name_map[] = {
        { "object",  TJV_VALIDATION_EX_OBJECT  },
        { "array",   TJV_VALIDATION_EX_ARRAY   },
        { "list",    TJV_VALIDATION_EX_ARRAY   },
        { "string",  TJV_VALIDATION_EX_STRING  },
        { "integer", TJV_VALIDATION_EX_INTEGER },
        { "json",    TJV_VALIDATION_EX_JSON    },
        { "boolean", TJV_VALIDATION_EX_BOOLEAN },
        { "double",  TJV_VALIDATION_EX_DOUBLE  },
        { "email",   TJV_VALIDATION_EX_EMAIL   },
        { NULL }
    };

    static const struct {
        const char *match_name;
        tjv_ValidationStringMatchingType match;
    } match_name_map[] = {
        { "glob",   TJV_STRING_MATCHING_GLOB  },
        { "regexp", TJV_STRING_MATCHING_REGEXP },
        { "list",   TJV_STRING_MATCHING_LIST },
        { NULL }
    };

    DBG2(printf("parse arguments"));

    Tcl_Size temp_objc = objc;
    // If last_arg is NULL, then we should not accept anything unknown. Let's pass NULL
    // in the last argument for this case.
    if (Tcl_ParseArgsObjv(interp, ArgTable, &temp_objc, objv, (last_arg == NULL ? NULL : &remObjv)) != TCL_OK) {
        DBG2(printf("return: ERROR (failed to parse args)"));
        goto error;
    }

    // If we need to return the last unknown argument, then check it now.
    // Also, let's check that we have only one unknown argument.
    if (remObjv != NULL) {
        if (temp_objc == 2) {
            DBG2(printf("found 1 extra argument: [%s]", Tcl_GetString(remObjv[1])));
            // We may not check if last_arg is NULL. If it is NULL, we will not pass remObjv
            // to Tcl_ParseArgsObjv() and it will remain as NULL. Therefore, we will not go
            // into the current condition.
            *last_arg = remObjv[1];
        } else if (temp_objc > 2) {
            DBG2(printf("return: ERROR (%" TCL_SIZE_MODIFIER "d extra argument(s))", temp_objc - 1));
            Tcl_SetObjResult(interp, Tcl_ObjPrintf("unrecognized argument \"%s\"",
                Tcl_GetString(remObjv[2])));
            goto error;
        }
    }

    // Validate parameters

    if (opt_type == NULL || opt_type == INT2PTR(1)) {
        DBG2(printf("return: ERROR (no -type)"));
        SetResult("required option -type is not specified or its value is missing");
        goto error;
    }

    if (Tcl_GetIndexFromObjStruct(interp, opt_type, type_name_map,
            sizeof(type_name_map[0]), "type", 0, &idx) != TCL_OK)
    {
        DBG2(printf("return: ERROR (wrong -type: [%s])", Tcl_GetString(opt_type)));
        goto error;
    }

    tjv_ValidationElementTypeEx element_type = type_name_map[idx].type;

    // Check if we have options with missing values

    const char *bad_option = NULL;
    if (opt_command == INT2PTR(1)) {
        bad_option = "-command";
    } else if (opt_pattern == INT2PTR(1)) {
        bad_option = "-pattern";
    } else if (opt_minimum == INT2PTR(1)) {
        bad_option = "-minimum";
    } else if (opt_maximum == INT2PTR(1)) {
        bad_option = "-maximum";
    } else if (opt_properties == INT2PTR(1)) {
        bad_option = "-properties";
    } else if (opt_match == INT2PTR(1)) {
        bad_option = "-match";
    } else if (opt_items == INT2PTR(1)) {
        bad_option = "-items";
    }

    if (bad_option != NULL) {
        DBG2(printf("return: ERROR (no arg for %s)", bad_option));
        Tcl_SetObjResult(interp, Tcl_ObjPrintf("\"%s\" option requires"
            " an additional argument", bad_option));
        goto error;
    }

    // Check if the user has specified options that are not supported for
    // the corresponding type.

    if (opt_match != NULL && element_type != TJV_VALIDATION_EX_STRING) {
        bad_option = "-match";
    } else if (opt_pattern != NULL && element_type != TJV_VALIDATION_EX_STRING) {
        bad_option = "-pattern";
    } else if (opt_properties != NULL && !(element_type == TJV_VALIDATION_EX_OBJECT || element_type == TJV_VALIDATION_EX_JSON)) {
        bad_option = "-properties";
    } else if (opt_minimum != NULL && !(element_type == TJV_VALIDATION_EX_INTEGER || element_type == TJV_VALIDATION_EX_DOUBLE)) {
        bad_option = "-minimum";
    } else if (opt_maximum != NULL && !(element_type == TJV_VALIDATION_EX_INTEGER || element_type == TJV_VALIDATION_EX_DOUBLE)) {
        bad_option = "-maximum";
    } else if (opt_items != NULL && !(element_type == TJV_VALIDATION_EX_ARRAY || element_type == TJV_VALIDATION_EX_JSON)) {
        bad_option = "-items";
    }

    if (bad_option != NULL) {
        DBG2(printf("return: ERROR (wrong option %s for type %s)", bad_option, Tcl_GetString(opt_type)));
        Tcl_SetObjResult(interp, Tcl_ObjPrintf("\"%s\" option is not supported for type \"%s\"",
            bad_option, Tcl_GetString(opt_type)));
        goto error;
    }

    // Check if callback specified. Validation options will not work in this case.

    if (opt_command != NULL) {
        if (opt_match != NULL) {
            bad_option = "-match";
        } else if (opt_pattern != NULL) {
            bad_option = "-pattern";
        } else if (opt_minimum != NULL) {
            bad_option = "-minimum";
        } else if (opt_maximum != NULL) {
            bad_option = "-maximum";
        }
    }

    if (bad_option != NULL) {
        DBG2(printf("return: ERROR (option %s present when callback specified)", bad_option));
        Tcl_SetObjResult(interp, Tcl_ObjPrintf("\"%s\" option is specified when"
            " a validation callback is specified", bad_option));
        goto error;
    }

    // Validate type-specific options
    DBG2(printf("create new element type: %s (required: %s; nullable: %s; callback: %s)",
        TJV_VALIDATION_EX_TYPE_STR(element_type), (opt_is_required ? "yes" : "no"),
        (opt_is_nullable ? "yes" : "no"), (opt_command == NULL ? "no" : "yes")));

    rc = tjv_ValidationElementAlloc(element_type);
    rc->is_required = opt_is_required;
    rc->is_nullable = opt_is_nullable;
    if (opt_command != NULL) {
        rc->command = opt_command;
        Tcl_IncrRefCount(rc->command);
    }

    ThreadSpecificData *tsdPtr;

    switch (element_type) {
    case TJV_VALIDATION_EX_EMAIL:

        tsdPtr = TCL_TSD_INIT(&dataKey);
        if (tsdPtr->email_pattern == NULL) {
            tsdPtr->email_pattern = Tcl_NewStringObj(TJV_REGEXP_EMAIL, -1);
            Tcl_IncrRefCount(tsdPtr->email_pattern);
            tsdPtr->email_regexp = Tcl_GetRegExpFromObj(interp, tsdPtr->email_pattern, 0);
        }

        rc->opts.str_type.match = TJV_STRING_MATCHING_REGEXP;
        rc->opts.str_type.pattern = tsdPtr->email_pattern;
        Tcl_IncrRefCount(rc->opts.str_type.pattern);
        rc->opts.str_type.regexp = tsdPtr->email_regexp;

        break;
    case TJV_VALIDATION_EX_STRING:

        if (opt_match != NULL) {
            if (opt_pattern == NULL) {
                DBG2(printf("return: ERROR (-match without -pattern)"));
                SetResult("option -match is specified, but -pattern is missing");
                goto error;
            }
            if (Tcl_GetIndexFromObjStruct(interp, opt_match, match_name_map,
                sizeof(match_name_map[0]), "matching type", 0, &idx) != TCL_OK)
            {
                DBG2(printf("return: ERROR (wrong -match: [%s])", Tcl_GetString(opt_match)));
                goto error;
            }
            rc->opts.str_type.match = match_name_map[idx].match;
            DBG2(printf("matching type: %d", (int)rc->opts.str_type.match));
        } else {
            DBG2(printf("matching type: regexp (default)"));
            rc->opts.str_type.match = TJV_STRING_MATCHING_REGEXP;
        }

        if (opt_pattern != NULL) {

            DBG2(printf("pattern: [%s]", Tcl_GetString(opt_pattern)));

            // If a pattern is defined, we first create a copy of the pattern and then try
            // to compile it. We then store this copy in our structures.
            // We cannot store original object, because compiled regexp is available
            // as long as the internal representation of bound Tcl object
            // is not changed.
            //
            // For glob and list maching types, we can safely use the original pattern.

            if (rc->opts.str_type.match == TJV_STRING_MATCHING_REGEXP) {

                rc->opts.str_type.pattern = Tcl_DuplicateObj(opt_pattern);
                Tcl_IncrRefCount(rc->opts.str_type.pattern);
                rc->opts.str_type.regexp = Tcl_GetRegExpFromObj(interp, rc->opts.str_type.pattern, 0);
                if (rc->opts.str_type.regexp == NULL) {
                    DBG2(printf("return: ERROR (could not compile regexp [%s])",
                        Tcl_GetString(rc->opts.str_type.pattern)));
                    goto error;
                }

            } else {
                rc->opts.str_type.pattern = opt_pattern;
                Tcl_IncrRefCount(rc->opts.str_type.pattern);
            }

        } else {
            DBG2(printf("pattern: not defined"));
        }

        break;
    case TJV_VALIDATION_EX_INTEGER:
        if (opt_minimum != NULL) {
            if (Tcl_GetWideIntFromObj(interp, opt_minimum, &rc->opts.int_type.min_value) != TCL_OK) {
                DBG2(printf("return: ERROR (wrong -minimum value: [%s])", Tcl_GetString(opt_minimum)));
                goto error;
            }
            rc->opts.int_type.is_min_value_defined = 1;
            DBG2(printf("min val: %" TCL_LL_MODIFIER "d", rc->opts.int_type.min_value));
        } else {
            DBG2(printf("min val: not defined"));
        }
        if (opt_maximum != NULL) {
            if (Tcl_GetWideIntFromObj(interp, opt_maximum, &rc->opts.int_type.max_value) != TCL_OK) {
                DBG2(printf("return: ERROR (wrong -maximum value: [%s])", Tcl_GetString(opt_maximum)));
                goto error;
            }
            rc->opts.int_type.is_max_value_defined = 1;
            DBG2(printf("max val: %" TCL_LL_MODIFIER "d", rc->opts.int_type.max_value));
        } else {
            DBG2(printf("max val: not defined"));
        }
        break;
    case TJV_VALIDATION_EX_JSON:

        if (opt_items != NULL) {

            if (opt_properties != NULL) {
                DBG2(printf("return: error (both -items and -properties are defined)"));
                SetResult("both options -items and -properties are specified,"
                    " for json format only one of them can be specified");
                goto error;
            }

            DBG2(printf("add items format"));
            if (tjv_ValidationCompileItems(interp, opt_items, &rc->opts.json_type.element) != TCL_OK) {
                DBG2(printf("return: error (failed to parse items format)"));
                goto error;
            }

        } else if (opt_properties != NULL) {

            DBG2(printf("add properties"));
            if (tjv_ValidationCompileProperties(interp, opt_properties, &rc->opts.json_type.keys_list, &rc->opts.json_type.elements) != TCL_OK) {
                DBG2(printf("return: error (failed to parse properties)"));
                goto error;
            }

        } else {
            DBG2(printf("items or properties are not specified"));
        }

        break;
    case TJV_VALIDATION_EX_ARRAY:

        if (opt_items != NULL) {

            DBG2(printf("add items format"));
            if (tjv_ValidationCompileItems(interp, opt_items, &rc->opts.array_type.element) != TCL_OK) {
                DBG2(printf("return: error (failed to parse items format)"));
                goto error;
            }

        } else {
            DBG2(printf("items format is not specified"));
        }

        break;
    case TJV_VALIDATION_EX_OBJECT:

        if (opt_properties != NULL) {

            DBG2(printf("add properties"));
            if (tjv_ValidationCompileProperties(interp, opt_properties, &rc->opts.obj_type.keys_list, &rc->opts.obj_type.elements) != TCL_OK) {
                DBG2(printf("return: error (failed to parse properties)"));
                goto error;
            }

        } else {
            DBG2(printf("no properties are defined"));
        }

        break;
    case TJV_VALIDATION_EX_BOOLEAN:
        break;
    case TJV_VALIDATION_EX_DOUBLE:
        if (opt_minimum != NULL) {
            if (Tcl_GetDoubleFromObj(interp, opt_minimum, &rc->opts.double_type.min_value) != TCL_OK) {
                DBG2(printf("return: ERROR (wrong -minimum value: [%s])", Tcl_GetString(opt_minimum)));
                goto error;
            }
            rc->opts.double_type.is_min_value_defined = 1;
            DBG2(printf("min val: %f", rc->opts.double_type.min_value));
        } else {
            DBG2(printf("min val: not defined"));
        }
        if (opt_maximum != NULL) {
            if (Tcl_GetDoubleFromObj(interp, opt_maximum, &rc->opts.double_type.max_value) != TCL_OK) {
                DBG2(printf("return: ERROR (wrong -maximum value: [%s])", Tcl_GetString(opt_maximum)));
                goto error;
            }
            rc->opts.double_type.is_max_value_defined = 1;
            DBG2(printf("max val: %f", rc->opts.double_type.max_value));
        } else {
            DBG2(printf("max val: not defined"));
        }
        break;
    }

    DBG2(printf("return: ok (%p)", (void *)rc));
    goto done;

error:
    if (rc != NULL) {
        tjv_ValidationElementFree(rc);
        rc = NULL;
    }

done:
    if (remObjv != NULL) {
        ckfree(remObjv);
    }
    return rc;

}

static void tjv_ValidationCompileThreadExitProc(ClientData clientData) {

    UNUSED(clientData);

    ThreadSpecificData *tsdPtr = TCL_TSD_INIT(&dataKey);

    DBG2(printf("enter..."));

    if (tsdPtr->email_pattern != NULL) {
        Tcl_DecrRefCount(tsdPtr->email_pattern);
        tsdPtr->email_pattern = NULL;
    }

    DBG2(printf("return: ok"));

}

void tjv_ValidationCompileInit(void) {

    Tcl_MutexLock(&tjv_ValidationCompile_initialize_mx);

    if (!tjv_ValidationCompile_initialized) {
        DBG2(printf("enter..."));
        Tcl_CreateThreadExitHandler(tjv_ValidationCompileThreadExitProc, NULL);
        tjv_ValidationCompile_initialized = 1;
        DBG2(printf("return: ok"));
    }

    Tcl_MutexUnlock(&tjv_ValidationCompile_initialize_mx);
}
