/**
 * Copyright Jerily LTD. All Rights Reserved.
 * SPDX-FileCopyrightText: 2024 Neofytos Dimitriou (neo@jerily.cy)
 * SPDX-License-Identifier: MIT.
 */

#include "tjvCompile.h"

static Tcl_ThreadDataKey dataKey;

#define TCL_TSD_INIT(keyPtr) \
    (ThreadSpecificData *)Tcl_GetThreadData((keyPtr), sizeof(ThreadSpecificData))

static int tjv_validationcompile_initialized = 0;
static Tcl_Mutex tjv_validationcompile_initialize_mx;

#define TJV_CUSTOM_TYPE_COUNT 12

static const struct {
    tjv_ValidationElementTypeEx type_ex;
    const char *name;
    const char *pattern;
} tjv_custom_types[TJV_CUSTOM_TYPE_COUNT] = {
    {
        TJV_VALIDATION_EX_EMAIL,
        "email",
        "(?i)^[a-z0-9!#$%&'*+/=?^_`{|}~-]+(?:\\.[a-z0-9!#$%&'*+/=?^_`{|}~-]+)*@(?:[a-z0-9](?:[a-z0-9-]*[a-z0-9])?\\.)+[a-z0-9](?:[a-z0-9-]*[a-z0-9])?$"
    }, {
        TJV_VALIDATION_EX_DURATION,
        "duration",
        "^P(?!$)((\\d+Y)?(\\d+M)?(\\d+D)?(T(?=\\d)(\\d+H)?(\\d+M)?(\\d+S)?)?|(\\d+W)?)$"
    }, {
        TJV_VALIDATION_EX_URI,
        "uri",
        "(?i)^(?:[a-z][a-z0-9+\\-.]*:)?(?:\\/?\\/(?:(?:[a-z0-9\\-._~!$&'()*+,;=:]|%[0-9a-f]{2})*@)?(?:\\[(?:(?:(?:(?:[0-9a-f]{1,4}:){6}|::(?:[0-9a-f]{1,4}:){5}|(?:[0-9a-f]{1,4})?::(?:[0-9a-f]{1,4}:){4}|(?:(?:[0-9a-f]{1,4}:){0,1}[0-9a-f]{1,4})?::(?:[0-9a-f]{1,4}:){3}|(?:(?:[0-9a-f]{1,4}:){0,2}[0-9a-f]{1,4})?::(?:[0-9a-f]{1,4}:){2}|(?:(?:[0-9a-f]{1,4}:){0,3}[0-9a-f]{1,4})?::[0-9a-f]{1,4}:|(?:(?:[0-9a-f]{1,4}:){0,4}[0-9a-f]{1,4})?::)(?:[0-9a-f]{1,4}:[0-9a-f]{1,4}|(?:(?:25[0-5]|2[0-4]\\d|[01]?\\d\\d?)\\.){3}(?:25[0-5]|2[0-4]\\d|[01]?\\d\\d?))|(?:(?:[0-9a-f]{1,4}:){0,5}[0-9a-f]{1,4})?::[0-9a-f]{1,4}|(?:(?:[0-9a-f]{1,4}:){0,6}[0-9a-f]{1,4})?::)|[Vv][0-9a-f]+\\.[a-z0-9\\-._~!$&'()*+,;=:]+)\\]|(?:(?:25[0-5]|2[0-4]\\d|[01]?\\d\\d?)\\.){3}(?:25[0-5]|2[0-4]\\d|[01]?\\d\\d?)|(?:[a-z0-9\\-._~!$&'\"()*+,;=]|%[0-9a-f]{2})*)(?::\\d*)?(?:\\/(?:[a-z0-9\\-._~!$&'\"()*+,;=:@]|%[0-9a-f]{2})*)*|\\/(?:(?:[a-z0-9\\-._~!$&'\"()*+,;=:@]|%[0-9a-f]{2})+(?:\\/(?:[a-z0-9\\-._~!$&'\"()*+,;=:@]|%[0-9a-f]{2})*)*)?|(?:[a-z0-9\\-._~!$&'\"()*+,;=:@]|%[0-9a-f]{2})+(?:\\/(?:[a-z0-9\\-._~!$&'\"()*+,;=:@]|%[0-9a-f]{2})*)*)?(?:\\?(?:[a-z0-9\\-._~!$&'\"()*+,;=:@/?]|%[0-9a-f]{2})*)?(?:#(?:[a-z0-9\\-._~!$&'\"()*+,;=:@/?]|%[0-9a-f]{2})*)?$"
    }, {
        TJV_VALIDATION_EX_URI_TEMPLATE,
        "uri-template",
        "(?i)^(?:(?:[^\\x00-\\x20\"'<>%\\\\^`{|}]|%[0-9a-f]{2})|\\{[+#./;?&=,!@|]?(?:[a-z0-9_]|%[0-9a-f]{2})+(?::[1-9][0-9]{0,3}|\\*)?(?:,(?:[a-z0-9_]|%[0-9a-f]{2})+(?::[1-9][0-9]{0,3}|\\*)?)*\\})*$"
    }, {
        TJV_VALIDATION_EX_URL,
        "url",
        "(?i)^(?:https?|ftp):\\/\\/(?:\\S+(?::\\S*)?@)?(?:(?!(?:10|127)(?:\\.\\d{1,3}){3})(?!(?:169\\.254|192\\.168)(?:\\.\\d{1,3}){2})(?!172\\.(?:1[6-9]|2\\d|3[0-1])(?:\\.\\d{1,3}){2})(?:[1-9]\\d?|1\\d\\d|2[01]\\d|22[0-3])(?:\\.(?:1?\\d{1,2}|2[0-4]\\d|25[0-5])){2}(?:\\.(?:[1-9]\\d?|1\\d\\d|2[0-4]\\d|25[0-4]))|(?:(?:[a-z0-9\\u00a1-\\uffff]+-)*[a-z0-9\\u00a1-\\uffff]+)(?:\\.(?:[a-z0-9\\u00a1-\\uffff]+-)*[a-z0-9\\u00a1-\\uffff]+)*(?:\\.(?:[a-z\\u00a1-\\uffff]{2,})))(?::\\d{2,5})?(?:\\/[^\\s]*)?$"
    }, {
        TJV_VALIDATION_EX_HOSTNAME,
        "hostname",
        "(?i)^(?=.{1,253}\\.?$)[a-z0-9](?:[a-z0-9-]{0,61}[a-z0-9])?(?:\\.[a-z0-9](?:[-0-9a-z]{0,61}[0-9a-z])?)*\\.?$"
    }, {
        TJV_VALIDATION_EX_IPV4,
        "ipv4",
        "^(?:(?:25[0-5]|2[0-4]\\d|1\\d\\d|[1-9]?\\d)\\.){3}(?:25[0-5]|2[0-4]\\d|1\\d\\d|[1-9]?\\d)$"
    }, {
        TJV_VALIDATION_EX_IPV6,
        "ipv6",
        "(?i)^((([0-9a-f]{1,4}:){7}([0-9a-f]{1,4}|:))|(([0-9a-f]{1,4}:){6}(:[0-9a-f]{1,4}|((25[0-5]|2[0-4]\\d|1\\d\\d|[1-9]?\\d)(\\.(25[0-5]|2[0-4]\\d|1\\d\\d|[1-9]?\\d)){3})|:))|(([0-9a-f]{1,4}:){5}(((:[0-9a-f]{1,4}){1,2})|:((25[0-5]|2[0-4]\\d|1\\d\\d|[1-9]?\\d)(\\.(25[0-5]|2[0-4]\\d|1\\d\\d|[1-9]?\\d)){3})|:))|(([0-9a-f]{1,4}:){4}(((:[0-9a-f]{1,4}){1,3})|((:[0-9a-f]{1,4})?:((25[0-5]|2[0-4]\\d|1\\d\\d|[1-9]?\\d)(\\.(25[0-5]|2[0-4]\\d|1\\d\\d|[1-9]?\\d)){3}))|:))|(([0-9a-f]{1,4}:){3}(((:[0-9a-f]{1,4}){1,4})|((:[0-9a-f]{1,4}){0,2}:((25[0-5]|2[0-4]\\d|1\\d\\d|[1-9]?\\d)(\\.(25[0-5]|2[0-4]\\d|1\\d\\d|[1-9]?\\d)){3}))|:))|(([0-9a-f]{1,4}:){2}(((:[0-9a-f]{1,4}){1,5})|((:[0-9a-f]{1,4}){0,3}:((25[0-5]|2[0-4]\\d|1\\d\\d|[1-9]?\\d)(\\.(25[0-5]|2[0-4]\\d|1\\d\\d|[1-9]?\\d)){3}))|:))|(([0-9a-f]{1,4}:){1}(((:[0-9a-f]{1,4}){1,6})|((:[0-9a-f]{1,4}){0,4}:((25[0-5]|2[0-4]\\d|1\\d\\d|[1-9]?\\d)(\\.(25[0-5]|2[0-4]\\d|1\\d\\d|[1-9]?\\d)){3}))|:))|(:(((:[0-9a-f]{1,4}){1,7})|((:[0-9a-f]{1,4}){0,5}:((25[0-5]|2[0-4]\\d|1\\d\\d|[1-9]?\\d)(\\.(25[0-5]|2[0-4]\\d|1\\d\\d|[1-9]?\\d)){3}))|:)))$"
    }, {
        TJV_VALIDATION_EX_UUID,
        "uuid",
        "(?i)^(?:urn:uuid:)?[0-9a-f]{8}-(?:[0-9a-f]{4}-){3}[0-9a-f]{12}$"
    }, {
        TJV_VALIDATION_EX_JSON_POINTER,
        "json-pointer",
        "^(?:\\/(?:[^~/]|~0|~1)*)*$"
    }, {
        TJV_VALIDATION_EX_JSON_POINTER_URI_FRAGMENT,
        "json-pointer-uri-fragment",
        "(?i)^#(?:\\/(?:[a-z0-9_\\-.!$&'()*+,;:=@]|%[0-9a-f]{2}|~0|~1)*)*$"
    }, {
        TJV_VALIDATION_EX_RELATIVE_JSON_POINTER,
        "relative-json-pointer",
        "^(?:0|[1-9][0-9]*)(?:#|(?:\\/(?:[^~/]|~0|~1)*)*)$"
    }
};

typedef struct ThreadSpecificData {
    Tcl_Obj *pattern[TJV_CUSTOM_TYPE_COUNT];
    Tcl_RegExp regexp[TJV_CUSTOM_TYPE_COUNT];
} ThreadSpecificData;

static int tjv_GetCustomTypeId(tjv_ValidationElementTypeEx type_ex) {

    for (int i = 0; i < TJV_CUSTOM_TYPE_COUNT; i++) {
        if (tjv_custom_types[i].type_ex == type_ex) {
            return i;
        }
    }

    return -1;

}

const char *tjv_GetValidationTypeString(tjv_ValidationElementTypeEx type_ex) {
    switch (type_ex) {
    case TJV_VALIDATION_EX_OBJECT:
        return "object";
    case TJV_VALIDATION_EX_ARRAY:
        return "array";
    case TJV_VALIDATION_EX_STRING:
        return "string";
    case TJV_VALIDATION_EX_INTEGER:
        return "integer";
    case TJV_VALIDATION_EX_JSON:
        return "json";
    case TJV_VALIDATION_EX_BOOLEAN:
        return "boolean";
    case TJV_VALIDATION_EX_DOUBLE:
        return "double";
    case TJV_VALIDATION_EX_EMAIL:
    case TJV_VALIDATION_EX_DURATION:
    case TJV_VALIDATION_EX_URI:
    case TJV_VALIDATION_EX_URI_TEMPLATE:
    case TJV_VALIDATION_EX_URL:
    case TJV_VALIDATION_EX_HOSTNAME:
    case TJV_VALIDATION_EX_IPV4:
    case TJV_VALIDATION_EX_IPV6:
    case TJV_VALIDATION_EX_UUID:
    case TJV_VALIDATION_EX_JSON_POINTER:
    case TJV_VALIDATION_EX_JSON_POINTER_URI_FRAGMENT:
    case TJV_VALIDATION_EX_RELATIVE_JSON_POINTER:
        return tjv_custom_types[tjv_GetCustomTypeId(type_ex)].name;
    }
    return NULL;
}

static tjv_ValidationElement *tjv_ValidationElementAlloc(tjv_ValidationElementTypeEx type) {

    tjv_ValidationElement *rc = ckalloc(sizeof(tjv_ValidationElement));
    memset(rc, 0, sizeof(tjv_ValidationElement));

    rc->type_ex = type;
    rc->type = TJV_VALIDATION_TYPE_FROM_EX(type);
    assert(rc->type != (unsigned)-1 && "unable to convert tjv_ValidationElementTypeEx to tjv_ValidationElementType");

    return rc;

}

void tjv_ValidationElementFree(tjv_ValidationElement *ve) {

    DBG2(printf("enter: ve: %p type: ", (void *)ve));

    if (ve->command != NULL) {
        Tcl_DecrRefCount(ve->command);
    }
    if (ve->key != NULL) {
        Tcl_DecrRefCount(ve->key);
    }
    if (ve->outkey != NULL) {
        Tcl_DecrRefCount(ve->outkey);
    }

    // A json can be defined as an array or an object. We need to change the ve
    // type to match the json type to properly release the children.
    if (ve->type == TJV_VALIDATION_JSON) {
        switch (ve->flag) {
        case TJV_FLAG_JSON_TYPE_OBJECT:
            DBG2(printf("set json type as an object"));
            ve->type = TJV_VALIDATION_OBJECT;
            break;
        case TJV_FLAG_JSON_TYPE_ARRAY:
            DBG2(printf("set json type as an array"));
            ve->type = TJV_VALIDATION_ARRAY;
            break;
        case TJV_FLAG_NONE:
        case TJV_FLAG_SKIP_KEY:
            break;
        }
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

static int tjv_ValidationCompileItems(Tcl_Interp *interp, Tcl_Obj *data, tjv_ValidationElement *ve) {

    DBG2(printf("enter"));

    // We will use a stub as the key of the validation element for array elements

    Tcl_Obj *items_format = Tcl_DuplicateObj(data);
    Tcl_Obj *key_stub = Tcl_NewStringObj("", -1);
    if (Tcl_ListObjReplace(interp, items_format, 0, 0, 1, &key_stub) != TCL_OK) {
        DBG2(printf("return: error (wrong list format)"));
        Tcl_BounceRefCount(items_format);
        Tcl_BounceRefCount(key_stub);
        return TCL_ERROR;
    }

    Tcl_Size items_objc;
    Tcl_Obj **items_objv;
    // We have already successfully inserted a stub parameter into items_format.
    // Therefore, we are confident that this is the correct list and do not check
    // for errors here.
    Tcl_ListObjGetElements(NULL, items_format, &items_objc, &items_objv);

    tjv_ValidationElement *element = tjv_ValidationCompile(interp, items_objc, items_objv, NULL, NULL);
    Tcl_BounceRefCount(items_format);

    if (element == NULL) {
        DBG2(printf("return: error (failed to items format)"));
        return TCL_ERROR;
    }

    if (element->type != TJV_VALIDATION_ARRAY) {
        element->flag = TJV_FLAG_SKIP_KEY;
    }

    ve->opts.array_type.element = element;

    DBG2(printf("return: ok"));
    return TCL_OK;

}

static int tjv_ValidationCompileProperties(Tcl_Interp *interp, Tcl_Obj *data, tjv_ValidationElement *ve) {

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
    ve->opts.obj_type.elements = elements;


    Tcl_Obj *keys_list = Tcl_NewListObj(0, NULL);
    Tcl_IncrRefCount(keys_list);
    ve->opts.obj_type.keys_list = keys_list;

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
        elements[i] = tjv_ValidationCompile(interp, child_objc, child_objv, NULL, NULL);
        if (elements[i] == NULL) {

            DBG2(printf("return: error (failed to parse element #%" TCL_SIZE_MODIFIER "d [%s]: %s",
                i, key_name, Tcl_GetStringResult(interp)));

            Tcl_SetObjResult(interp, Tcl_ObjPrintf("%s->%s", key_name, Tcl_GetStringResult(interp)));
            return TCL_ERROR;

        }

    }

    if (child_count > 0) {
        Tcl_ListObjGetElements(NULL, keys_list, &ve->opts.obj_type.keys_objc, &ve->opts.obj_type.keys_objv);
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

tjv_ValidationElement *tjv_ValidationCompile(Tcl_Interp *interp, Tcl_Size objc, Tcl_Obj *const objv[], Tcl_Obj **rest_arg1, Tcl_Obj **rest_arg2) {

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
    Tcl_Obj *opt_outkey = NULL;

#pragma GCC diagnostic push
// ignore warning for copy_arg:
//     warning: ISO C forbids conversion of function pointer to object pointer type [-Wpedantic]
#pragma GCC diagnostic ignored "-Wpedantic"
    Tcl_ArgvInfo ArgTable[] = {
        { TCL_ARGV_FUNC,     "-type",       copy_arg,   &opt_type,        NULL, NULL },
        { TCL_ARGV_CONSTANT, "-required",   INT2PTR(1), &opt_is_required, NULL, NULL },
        { TCL_ARGV_CONSTANT, "-nullable",   INT2PTR(1), &opt_is_nullable, NULL, NULL },
        // { TCL_ARGV_FUNC,     "-command",    copy_arg,   &opt_command,     NULL, NULL },
        { TCL_ARGV_FUNC,     "-outkey",     copy_arg,   &opt_outkey,      NULL, NULL },
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
        { "json",                      TJV_VALIDATION_EX_JSON                      },
        { "object",                    TJV_VALIDATION_EX_OBJECT                    },
        { "array",                     TJV_VALIDATION_EX_ARRAY                     },
        { "list",                      TJV_VALIDATION_EX_ARRAY                     },
        { "integer",                   TJV_VALIDATION_EX_INTEGER                   },
        { "float",                     TJV_VALIDATION_EX_DOUBLE                    },
        { "double",                    TJV_VALIDATION_EX_DOUBLE                    },
        { "boolean",                   TJV_VALIDATION_EX_BOOLEAN                   },
        { "string",                    TJV_VALIDATION_EX_STRING                    },
        { "email",                     TJV_VALIDATION_EX_EMAIL                     },
        { "duration",                  TJV_VALIDATION_EX_DURATION                  },
        { "uri",                       TJV_VALIDATION_EX_URI                       },
        { "uri-template",              TJV_VALIDATION_EX_URI_TEMPLATE              },
        { "url",                       TJV_VALIDATION_EX_URL                       },
        { "hostname",                  TJV_VALIDATION_EX_HOSTNAME                  },
        { "ipv4",                      TJV_VALIDATION_EX_IPV4                      },
        { "ipv6",                      TJV_VALIDATION_EX_IPV6                      },
        { "uuid",                      TJV_VALIDATION_EX_UUID                      },
        { "json-pointer",              TJV_VALIDATION_EX_JSON_POINTER              },
        { "json-pointer-uri-fragment", TJV_VALIDATION_EX_JSON_POINTER_URI_FRAGMENT },
        { "relative-json-pointer",     TJV_VALIDATION_EX_RELATIVE_JSON_POINTER     },
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
    // If rest_arg1 is NULL, then we should not accept anything unknown. Let's pass NULL
    // in the last argument for this case.
    if (Tcl_ParseArgsObjv(interp, ArgTable, &temp_objc, objv, (rest_arg1 == NULL ? NULL : &remObjv)) != TCL_OK) {
        DBG2(printf("return: ERROR (failed to parse args)"));
        goto error;
    }
    if (rest_arg1 != NULL) {
        DBG2(printf("arguments left: %" TCL_SIZE_MODIFIER "d", temp_objc));
    }

    // If we need to return the last unknown argument, then check it now.
    // Also, let's check that we have only one unknown argument.
    if (remObjv != NULL) {
        if (temp_objc > 1) {
            DBG2(printf("found 1st extra argument: [%s]", Tcl_GetString(remObjv[1])));
            // We may not check if rest_arg1 is NULL. If it is NULL, we will not pass remObjv
            // to Tcl_ParseArgsObjv() and it will remain as NULL. Therefore, we will not go
            // into the current condition.
            *rest_arg1 = remObjv[1];
        }
        if (temp_objc == 3 && rest_arg2 != NULL) {
            DBG2(printf("found 2nd extra argument: [%s]", Tcl_GetString(remObjv[2])));
            *rest_arg2 = remObjv[2];
        } else if (temp_objc > 2) {
            int extra_arg_idx = (rest_arg2 == NULL ? 2 : 3);
            DBG2(printf("return: ERROR (%" TCL_SIZE_MODIFIER "d extra argument(s))", temp_objc - extra_arg_idx - 1));
            Tcl_SetObjResult(interp, Tcl_ObjPrintf("unrecognized argument \"%s\"",
                Tcl_GetString(remObjv[extra_arg_idx])));
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
    } else if (opt_outkey == INT2PTR(1)) {
        bad_option = "-outkey";
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

    if (opt_outkey != NULL) {

        DBG2(printf("outkey: [%s]", Tcl_GetString(opt_outkey)));

        if (Tcl_ListObjGetElements(interp, opt_outkey, &rc->outkey_objc, &rc->outkey_objv) != TCL_OK) {
            DBG2(printf("return: ERROR (-outkey is not a list [%s])",
                Tcl_GetString(opt_outkey)));
            goto error;
        }

        rc->outkey = opt_outkey;
        Tcl_IncrRefCount(rc->outkey);

    } else {
        DBG2(printf("outkey: <none>"));
    }

    // If rest_arg1 is not NULL, then we are currently in the root element and
    // it should be without a key. Therefore, we do not change it, and leave it
    // as NULL by default. Otherwise, we use the first element as our key.
    if (rest_arg1 == NULL) {
        rc->key = objv[0];
        Tcl_IncrRefCount(rc->key);
        DBG2(printf("key: [%s]", Tcl_GetString(rc->key)));
    } else {
        DBG2(printf("key: <root>"));
    }

    ThreadSpecificData *tsdPtr;

    switch (element_type) {
    case TJV_VALIDATION_EX_HOSTNAME:
    case TJV_VALIDATION_EX_IPV4:
    case TJV_VALIDATION_EX_IPV6:
    case TJV_VALIDATION_EX_UUID:
    case TJV_VALIDATION_EX_JSON_POINTER:
    case TJV_VALIDATION_EX_JSON_POINTER_URI_FRAGMENT:
    case TJV_VALIDATION_EX_RELATIVE_JSON_POINTER:
    case TJV_VALIDATION_EX_URL:
    case TJV_VALIDATION_EX_URI:
    case TJV_VALIDATION_EX_URI_TEMPLATE:
    case TJV_VALIDATION_EX_DURATION:
    case TJV_VALIDATION_EX_EMAIL:

        tsdPtr = TCL_TSD_INIT(&dataKey);
        int type_id = tjv_GetCustomTypeId(element_type);
        if (tsdPtr->pattern[type_id] == NULL) {
            tsdPtr->pattern[type_id] = Tcl_NewStringObj(tjv_custom_types[type_id].pattern, -1);
            Tcl_IncrRefCount(tsdPtr->pattern[type_id]);
            tsdPtr->regexp[type_id] = Tcl_GetRegExpFromObj(NULL, tsdPtr->pattern[type_id], TCL_REG_ADVANCED);
            assert(tsdPtr->regexp[type_id] != NULL && "failed to compile regexp");
        }

        rc->opts.str_type.match = TJV_STRING_MATCHING_REGEXP;
        rc->opts.str_type.pattern = tsdPtr->pattern[type_id];
        Tcl_IncrRefCount(rc->opts.str_type.pattern);
        rc->opts.str_type.regexp = tsdPtr->regexp[type_id];

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
            // For the list type, we split the list and it should not be modified
            // so as not to invalidate the split form. Thus, we make a copy of
            // the pattern also.
            //
            // For glob type, we can safely use the original pattern.

            if (rc->opts.str_type.match == TJV_STRING_MATCHING_REGEXP) {

                rc->opts.str_type.pattern = Tcl_DuplicateObj(opt_pattern);
                Tcl_IncrRefCount(rc->opts.str_type.pattern);
                rc->opts.str_type.regexp = Tcl_GetRegExpFromObj(interp, rc->opts.str_type.pattern, TCL_REG_ADVANCED);
                if (rc->opts.str_type.regexp == NULL) {
                    DBG2(printf("return: ERROR (could not compile regexp [%s])",
                        Tcl_GetString(rc->opts.str_type.pattern)));
                    goto error;
                }

            } else if (rc->opts.str_type.match == TJV_STRING_MATCHING_LIST) {

                rc->opts.str_type.pattern = Tcl_DuplicateObj(opt_pattern);
                Tcl_IncrRefCount(rc->opts.str_type.pattern);
                if (Tcl_ListObjGetElements(interp, rc->opts.str_type.pattern, &rc->opts.str_type.pattern_objc, &rc->opts.str_type.pattern_objv) != TCL_OK) {
                    DBG2(printf("return: ERROR (not a list [%s])",
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

            rc->flag = TJV_FLAG_JSON_TYPE_ARRAY;

            DBG2(printf("add items format"));
            if (tjv_ValidationCompileItems(interp, opt_items, rc) != TCL_OK) {
                DBG2(printf("return: error (failed to parse items format)"));
                goto error;
            }

        } else if (opt_properties != NULL) {

            rc->flag = TJV_FLAG_JSON_TYPE_OBJECT;

            DBG2(printf("add properties"));
            if (tjv_ValidationCompileProperties(interp, opt_properties, rc) != TCL_OK) {
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
            if (tjv_ValidationCompileItems(interp, opt_items, rc) != TCL_OK) {
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
            if (tjv_ValidationCompileProperties(interp, opt_properties, rc) != TCL_OK) {
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

    for (int i = 0; i < TJV_CUSTOM_TYPE_COUNT; i++) {
        if (tsdPtr->pattern[i] != NULL) {
            Tcl_DecrRefCount(tsdPtr->pattern[i]);
            tsdPtr->pattern[i] = NULL;
        }
    }

    DBG2(printf("return: ok"));

}

void tjv_ValidationCompileInit(void) {

    Tcl_MutexLock(&tjv_validationcompile_initialize_mx);

    if (!tjv_validationcompile_initialized) {
        DBG2(printf("enter..."));
        Tcl_CreateThreadExitHandler(tjv_ValidationCompileThreadExitProc, NULL);
        tjv_validationcompile_initialized = 1;
        DBG2(printf("return: ok"));
    }

    Tcl_MutexUnlock(&tjv_validationcompile_initialize_mx);
}
