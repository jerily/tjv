/**
 * Copyright Jerily LTD. All Rights Reserved.
 * SPDX-FileCopyrightText: 2024 Neofytos Dimitriou (neo@jerily.cy)
 * SPDX-License-Identifier: MIT.
 */

#include "tjvMessage.h"

enum {
    TJV_STATIC_STR_ERROR,
    TJV_STATIC_STR_NAME,
    TJV_STATIC_STR_MESSAGE,
    TJV_STATIC_STR_DATA,
    TJV_STATIC_STR_KEYWORD,
    TJV_STATIC_STR_DATAPATH,
    TJV_STATIC_STR_NAME_VALUE,
    TJV_STATIC_STR_KEYWORD_REQUIRED,
    TJV_STATIC_STR_KEYWORD_TYPE,
    TJV_STATIC_STR_KEYWORD_VALUE,
    _TJV_STATIC_STR_COUNT
};

static const char *static_strings[_TJV_STATIC_STR_COUNT] = {
    "error", "name", "message", "data", "keyword", "dataPath",
    "ValidationError",
    "required", "type", "value"
};

typedef struct ThreadSpecificData {

    Tcl_Obj *static_strings[_TJV_STATIC_STR_COUNT];

    Tcl_Obj *key_error_name[2];
    Tcl_Obj *key_error_message[2];
    Tcl_Obj *key_error_data[2];


} ThreadSpecificData;

static Tcl_ThreadDataKey dataKey;

#define TCL_TSD_INIT(keyPtr) \
    (ThreadSpecificData *)Tcl_GetThreadData((keyPtr), sizeof(ThreadSpecificData))

static int tjv_message_initialized = 0;
static Tcl_Mutex tjv_message_initialize_mx;

static void tjv_MessageInitializeThreadData(ThreadSpecificData *tsdPtr) {

    DBG2(printf("enter..."));

    for (int i = 0; i < _TJV_STATIC_STR_COUNT; i++) {
        tsdPtr->static_strings[i] = Tcl_NewStringObj(static_strings[i], -1);
        Tcl_IncrRefCount(tsdPtr->static_strings[i]);
    }

    tsdPtr->key_error_name[0] = tsdPtr->static_strings[TJV_STATIC_STR_ERROR];
    tsdPtr->key_error_name[1] = tsdPtr->static_strings[TJV_STATIC_STR_NAME];

    tsdPtr->key_error_message[0] = tsdPtr->static_strings[TJV_STATIC_STR_ERROR];
    tsdPtr->key_error_message[1] = tsdPtr->static_strings[TJV_STATIC_STR_MESSAGE];

    tsdPtr->key_error_data[0] = tsdPtr->static_strings[TJV_STATIC_STR_ERROR];
    tsdPtr->key_error_data[1] = tsdPtr->static_strings[TJV_STATIC_STR_DATA];

    DBG2(printf("return: ok"));

}

void tjv_MessageGenerateType(Tcl_Obj *path, Tcl_Size index, const char *required_type,
    Tcl_Obj **error_message_ptr, Tcl_Obj **error_details_ptr)
{
    tjv_MessageGenerate(TJV_MSG_KEYWORD_TYPE, path, index,
        Tcl_ObjPrintf("should be %s", required_type),
        error_message_ptr, error_details_ptr);
}

void tjv_MessageGenerateRequired(Tcl_Obj *path, Tcl_Size index, Tcl_Obj *required_key,
    Tcl_Obj **error_message_ptr, Tcl_Obj **error_details_ptr)
{
    tjv_MessageGenerate(TJV_MSG_KEYWORD_REQUIRED, path, index,
        Tcl_ObjPrintf("should have required property '%s'", Tcl_GetString(required_key)),
        error_message_ptr, error_details_ptr);
}

void tjv_MessageGenerateValue(Tcl_Obj *path, Tcl_Size index, Tcl_Obj *message,
    Tcl_Obj **error_message_ptr, Tcl_Obj **error_details_ptr)
{
    tjv_MessageGenerate(TJV_MSG_KEYWORD_VALUE, path, index,
        message,
        error_message_ptr, error_details_ptr);
}

void tjv_MessageGenerate(tjv_MessageErrorKeywordType keyword_type, Tcl_Obj *path, Tcl_Size index, Tcl_Obj *message,
    Tcl_Obj **error_message_ptr, Tcl_Obj **error_details_ptr)
{

    DBG2(printf("enter: path: [%s] index: %" TCL_SIZE_MODIFIER "d", Tcl_GetString(path), index));

    Tcl_Obj *error_message = *error_message_ptr;
    if (error_message == NULL) {
        DBG2(printf("initialize messages list"));
        error_message = Tcl_NewListObj(0, NULL);
        *error_message_ptr = error_message;
    }

    Tcl_Obj *error_details = *error_details_ptr;
    if (error_details == NULL) {
        DBG2(printf("initialize details list"));
        error_details = Tcl_NewListObj(0, NULL);
        *error_details_ptr = error_details;
    }

    ThreadSpecificData *tsdPtr = TCL_TSD_INIT(&dataKey);
    if (tsdPtr->static_strings[0] == NULL) {
        tjv_MessageInitializeThreadData(tsdPtr);
    }

    if (index >= 0) {
        path = Tcl_DuplicateObj(path);
        char buf[32];
        snprintf(buf, sizeof(buf), "[%" TCL_SIZE_MODIFIER "d]", index);
        Tcl_AppendToObj(path, buf, -1);
    }

    Tcl_Obj *message_full;
    if (Tcl_GetCharLength(path) > 0) {
        message_full = Tcl_DuplicateObj(path);
        Tcl_AppendToObj(message_full, " ", 1);
        Tcl_AppendObjToObj(message_full, message);
    } else {
        message_full = message;
    }

    DBG2(printf("add message: [%s]", Tcl_GetString(message_full)));
    Tcl_ListObjAppendElement(NULL, error_message, message_full);

    Tcl_Obj *details = Tcl_NewDictObj();

    Tcl_Obj *keyword;
    switch (keyword_type) {
    case TJV_MSG_KEYWORD_TYPE:
        keyword = tsdPtr->static_strings[TJV_STATIC_STR_KEYWORD_TYPE];
        break;
    case TJV_MSG_KEYWORD_REQUIRED:
        keyword = tsdPtr->static_strings[TJV_STATIC_STR_KEYWORD_REQUIRED];
        break;
    case TJV_MSG_KEYWORD_VALUE:
        keyword = tsdPtr->static_strings[TJV_STATIC_STR_KEYWORD_VALUE];
        break;
    }
    Tcl_DictObjPut(NULL, details, tsdPtr->static_strings[TJV_STATIC_STR_KEYWORD], keyword);
    Tcl_DictObjPut(NULL, details, tsdPtr->static_strings[TJV_STATIC_STR_DATAPATH], path);
    Tcl_DictObjPut(NULL, details, tsdPtr->static_strings[TJV_STATIC_STR_MESSAGE], message);

    DBG2(printf("add details: [%s]", Tcl_GetString(details)));
    Tcl_ListObjAppendElement(NULL, error_details, details);

    DBG2(printf("return: ok"));

}

Tcl_Obj *tjv_MessageCombine(Tcl_Obj *error_message) {

    Tcl_Obj *rc = Tcl_NewStringObj("Error while validating data: ", -1);

    Tcl_Size objc;
    Tcl_Obj **objv;
    Tcl_ListObjGetElements(NULL, error_message, &objc, &objv);

    for (Tcl_Size i = 0; i < objc; i++) {

        if (i > 0) {
            Tcl_AppendToObj(rc, ", ", 2);
        }
        Tcl_AppendObjToObj(rc, objv[i]);

    }

    Tcl_BounceRefCount(error_message);

    return rc;

}

Tcl_Obj *tjv_MessageCombineDetails(Tcl_Obj *error_message, Tcl_Obj *error_details) {

    ThreadSpecificData *tsdPtr = TCL_TSD_INIT(&dataKey);

    Tcl_Obj *rc = Tcl_NewDictObj();
    Tcl_DictObjPutKeyList(NULL, rc, 2, tsdPtr->key_error_name, tsdPtr->static_strings[TJV_STATIC_STR_NAME_VALUE]);
    Tcl_DictObjPutKeyList(NULL, rc, 2, tsdPtr->key_error_message, tjv_MessageCombine(error_message));
    Tcl_DictObjPut(NULL, rc, tsdPtr->static_strings[TJV_STATIC_STR_DATA], error_details);

    return rc;

}

static void tjv_MessageThreadExitProc(ClientData clientData) {

    UNUSED(clientData);

    ThreadSpecificData *tsdPtr = TCL_TSD_INIT(&dataKey);

    DBG2(printf("enter..."));

    if (tsdPtr->static_strings[0] != NULL) {
        for (int i = 0; i < _TJV_STATIC_STR_COUNT; i++) {
            tsdPtr->static_strings[i] = Tcl_NewStringObj(static_strings[i], -1);
            Tcl_DecrRefCount(tsdPtr->static_strings[i]);
            tsdPtr->static_strings[i] = NULL;
        }
    }

    DBG2(printf("return: ok"));

}

void tjv_MessageInit(void) {

    Tcl_MutexLock(&tjv_message_initialize_mx);

    if (!tjv_message_initialized) {
        DBG2(printf("enter..."));
        Tcl_CreateThreadExitHandler(tjv_MessageThreadExitProc, NULL);
        tjv_message_initialized = 1;
        DBG2(printf("return: ok"));
    }

    Tcl_MutexUnlock(&tjv_message_initialize_mx);
}
