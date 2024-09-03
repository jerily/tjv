/**
 * Copyright Jerily LTD. All Rights Reserved.
 * SPDX-FileCopyrightText: 2024 Neofytos Dimitriou (neo@jerily.cy)
 * SPDX-License-Identifier: MIT.
 */

#include "library.h"

static Tcl_VarTraceProc tjv_HandleVarTraceProc;
static Tcl_CmdDeleteProc tjv_HandleDeleteProc;

static int tjv_ValidateCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]) {

    UNUSED(clientData);

    DBG2(printf("enter: objc: %d", objc));

    if (objc < 3) {
wrongArgsNum:
        Tcl_WrongNumArgs(interp, 1, objv, "validation_schema value ?outcome_variable?");
        DBG2(printf("return: TCL_ERROR (wrong # args)"));
        return TCL_ERROR;
    }

    Tcl_Obj *data = NULL;
    Tcl_Obj *outcome_var_name = NULL;
    int is_schema_compiled;
    tjv_ValidationElement *root;

    // Check if the first parameter looks like "::tjv::handle0x*". If this is the case,
    // then pre-compiled schema should be used.

    if (strncmp(Tcl_GetString(objv[1]), "::tjv::handle0x", 15) == 0) {

        is_schema_compiled = 1;

        DBG2(printf("use pre-compiled schema: [%s]", Tcl_GetString(objv[1])));

        if (objc > 4) {
            goto wrongArgsNum;
        }

        // Try to find an existing command and get the root validation item from
        // its clientData. If the command does not exist, it means that an invalid
        // pre-compiled validation scheme was specified.
        Tcl_CmdInfo cmd_info;
        if (Tcl_GetCommandInfo(interp, Tcl_GetString(objv[1]), &cmd_info) == 0) {
            DBG2(printf("return: TCL_ERROR (wrong pre-compiled schema [%s])", Tcl_GetString(objv[1])));
            Tcl_SetObjResult(interp, Tcl_ObjPrintf("pre-compiled validation schema \"%s\" does not exist", Tcl_GetString(objv[1])));
            return TCL_ERROR;
        }

        DBG2(printf("tjv_ValidationHandler: %p", (void *)cmd_info.objClientData));
        root = ((tjv_ValidationHandler *)cmd_info.objClientData)->root;
        DBG2(printf("tjv_ValidationElement: %p", (void *)root));

        data = objv[2];
        if (objc == 4) {
            outcome_var_name = objv[3];
        }

    } else {

        is_schema_compiled = 0;

        DBG2(printf("use inline validation schema"));
        root = tjv_ValidationCompile(interp, objc, objv, NULL, &data, &outcome_var_name);
        if (root == NULL) {
            DBG2(printf("return: TCL_ERROR"));
            return TCL_ERROR;
        }

        if (data == NULL) {
            DBG2(printf("no data to validate"));
            tjv_ValidationElementFree(root);
            goto wrongArgsNum;
        }

    }

    Tcl_Obj *error_message = NULL;
    Tcl_Obj *error_details = NULL;
    Tcl_Obj *outcome = Tcl_NewDictObj();

    tjv_ValidateTcl(data, -1, root, &error_message, &error_details, &outcome);

    if (!is_schema_compiled) {
        tjv_ValidationElementFree(root);
    }

    // Return ok if we don't have errors
    if (error_message == NULL) {
        if (outcome_var_name == NULL) {
            Tcl_SetObjResult(interp, outcome);
        } else {
            Tcl_ObjSetVar2(interp, outcome_var_name, NULL, outcome, 0);
            Tcl_SetObjResult(interp, Tcl_NewBooleanObj(1));
        }
        goto done;
    }

    // We don't need the outcome value in case of error
    Tcl_BounceRefCount(outcome);

    // If we don't have output variable, then return a message and TCL_ERROR
    if (outcome_var_name == NULL) {
        Tcl_SetObjResult(interp, tjv_MessageCombine(error_message));
        Tcl_BounceRefCount(error_details);
        DBG2(printf("return: TCL_ERROR"));
        return TCL_ERROR;
    }

    // Generate the error variable and return 0
    Tcl_ObjSetVar2(interp, outcome_var_name, NULL, tjv_MessageCombineDetails(error_message, error_details), 0);
    Tcl_SetObjResult(interp, Tcl_NewBooleanObj(0));
    DBG2(printf("return: 0 (with error variable)"));
    return TCL_OK;

done:

    DBG2(printf("return: ok"));

    return TCL_OK;

}


static int tjv_HandleCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]) {

    DBG2(printf("enter: objc: %d", objc));

    static const char *const commands[] = {
        "destroy", "validate",
        NULL
    };

    enum commands {
        cmdDestroy, cmdValidate
    };

    if (objc < 2) {
wrongArgsNum:
        Tcl_WrongNumArgs(interp, 1, objv, "validate value ?outcome_variable?");
        // Unfortunately, we do not have access to INTERP_ALTERNATE_WRONG_ARGS
        // from the extension. Let's simulate it.
        Tcl_AppendPrintfToObj(Tcl_GetObjResult(interp), " or \"%s destroy\"", Tcl_GetString(objv[0]));
        DBG2(printf("return: TCL_ERROR (wrong # args)"));
        return TCL_ERROR;
    }

    int command;
    if (Tcl_GetIndexFromObj(interp, objv[1], commands, "subcommand", 0, &command) != TCL_OK) {
        DBG2(printf("return: error (wrong subcommand: [%s])", Tcl_GetString(objv[1])));
        return TCL_ERROR;
    }

    tjv_ValidationHandler *h = (tjv_ValidationHandler *)clientData;

    if (command == cmdDestroy) {
        if (objc != 2) {
            goto wrongArgsNum;
        }
        DBG2(printf("destroy subcommand"));
        Tcl_SetObjResult(interp, h->cmd_name);
        Tcl_DeleteCommandFromToken(h->interp, h->cmd_token);
        goto done;
    }

    // If we are here, then we are in the validate subcommand. First, check
    // to see if we have enough arguments.
    if (objc < 3 || objc > 4) {
        goto wrongArgsNum;
    }

    Tcl_Obj *data = objv[2];
    Tcl_Obj *outcome_var_name = (objc == 3 ? NULL : objv[3]);
    DBG2(printf("outcome variable: [%s]", (outcome_var_name == NULL ? "<none>" : Tcl_GetString(outcome_var_name))));

    Tcl_Obj *error_message = NULL;
    Tcl_Obj *error_details = NULL;
    Tcl_Obj *outcome = Tcl_NewDictObj();

    tjv_ValidateTcl(data, -1, h->root, &error_message, &error_details, &outcome);

    // Return ok if we don't have errors
    if (error_message == NULL) {
        if (outcome_var_name == NULL) {
            Tcl_SetObjResult(interp, outcome);
        } else {
            Tcl_ObjSetVar2(interp, outcome_var_name, NULL, outcome, 0);
            Tcl_SetObjResult(interp, Tcl_NewBooleanObj(1));
        }
        goto done;
    }

    // We don't need the outcome value in case of error
    Tcl_BounceRefCount(outcome);

    // If we don't have output variable, then return a message and TCL_ERROR
    if (outcome_var_name == NULL) {
        Tcl_SetObjResult(interp, tjv_MessageCombine(error_message));
        Tcl_BounceRefCount(error_details);
        DBG2(printf("return: TCL_ERROR"));
        return TCL_ERROR;
    }

    // Generate the error variable and return 0
    Tcl_ObjSetVar2(interp, outcome_var_name, NULL, tjv_MessageCombineDetails(error_message, error_details), 0);
    Tcl_SetObjResult(interp, Tcl_NewBooleanObj(0));
    DBG2(printf("return: 0 (with error variable)"));
    return TCL_OK;

done:

    DBG2(printf("return: ok"));

    return TCL_OK;

}

static int tjv_CompileCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]) {

    DBG2(printf("enter: objc: %d", objc));

    UNUSED(clientData);
    if (objc < 3) {
        Tcl_WrongNumArgs(interp, 1, objv, "-type validation_type ?args? ?variable_name?");
        return TCL_ERROR;
    }

    Tcl_Obj *trace_variable_name = NULL;

    tjv_ValidationElement *root = tjv_ValidationCompile(interp, objc, objv, NULL, &trace_variable_name, NULL);
    if (root == NULL) {
        DBG2(printf("return: TCL_ERROR"));
        return TCL_ERROR;
    }

    tjv_ValidationHandler *h = ckalloc(sizeof(tjv_ValidationHandler));

    h->interp = interp;
    h->root = root;

    char buf[32];
    snprintf(buf, sizeof(buf), "%p", (void *)h);
    h->cmd_name = Tcl_ObjPrintf("::tjv::handle%s", buf);
    Tcl_IncrRefCount(h->cmd_name);

    h->cmd_token = Tcl_CreateObjCommand(interp, Tcl_GetString(h->cmd_name), tjv_HandleCmd,
        (ClientData)h, tjv_HandleDeleteProc);

    DBG2(printf("created command: %s", Tcl_GetString(h->cmd_name)));

    if (trace_variable_name != NULL) {
        h->trace_var = trace_variable_name;
        Tcl_IncrRefCount(h->trace_var);
        DBG2(printf("bind variable: %s", Tcl_GetString(h->trace_var)));
        Tcl_ObjSetVar2(interp, h->trace_var, NULL, h->cmd_name, 0);
        Tcl_TraceVar(interp, Tcl_GetString(h->trace_var),
            TCL_TRACE_WRITES | TCL_TRACE_UNSETS, tjv_HandleVarTraceProc, (ClientData)h);
    } else {
        h->trace_var = NULL;
    }

    Tcl_SetObjResult(interp, h->cmd_name);
    return TCL_OK;

}

#if TCL_MAJOR_VERSION > 8
#define MIN_VERSION "9.0"
#else
#define MIN_VERSION "8.6"
#endif

int Tjv_Init(Tcl_Interp *interp) {

    if (Tcl_InitStubs(interp, MIN_VERSION, 0) == NULL) {
        SetResult("Unable to initialize Tcl stubs");
        return TCL_ERROR;
    }

    tjv_ValidationCompileInit();
    tjv_MessageInit();

    Tcl_CreateNamespace(interp, "::tjv", NULL, NULL);
    Tcl_CreateObjCommand(interp, "::tjv::compile", tjv_CompileCmd, NULL, NULL);
    Tcl_CreateObjCommand(interp, "::tjv::validate", tjv_ValidateCmd, NULL, NULL);


    Tcl_RegisterConfig(interp, "tjv", tjv_pkgconfig, "iso8859-1");

    return Tcl_PkgProvide(interp, "tjv", XSTR(VERSION));

}

#ifdef USE_NAVISERVER
int Ns_ModuleInit(const char *server, const char *module) {
    Ns_TclRegisterTrace(server, (Ns_TclTraceProc *) Tjv_Init, server, NS_TCL_TRACE_CREATE);
    return NS_OK;
}
#endif

static char *tjv_HandleVarTraceProc(ClientData clientData, Tcl_Interp *interp,
    const char *name1, const char *name2, int flags)
{

    tjv_ValidationHandler *h = (tjv_ValidationHandler *)clientData;

    if (flags & TCL_TRACE_WRITES) {
        DBG2(printf("return: error (write attempt on var [%s])", name1));
        // Restore value
        Tcl_SetVar2(interp, name1, name2, Tcl_GetString(h->cmd_name), 0);
        return "readonly variable";
    }

    if (flags & TCL_TRACE_UNSETS) {
        DBG2(printf("unset var for handler: %p", (void *)h));
        Tcl_DeleteCommandFromToken(h->interp, h->cmd_token);
    }

    return NULL;

}

static void tjv_HandleDeleteProc(ClientData clientData) {

    tjv_ValidationHandler *h = (tjv_ValidationHandler *)clientData;

    DBG2(printf("delete command: %s", Tcl_GetString(h->cmd_name)));

    Tcl_DecrRefCount(h->cmd_name);

    if (h->trace_var != NULL) {
        DBG2(printf("untrace var: %s", Tcl_GetString(h->trace_var)));
        Tcl_UntraceVar(h->interp, Tcl_GetString(h->trace_var),
            TCL_TRACE_WRITES | TCL_TRACE_UNSETS, tjv_HandleVarTraceProc, clientData);
        Tcl_DecrRefCount(h->trace_var);
    } else {
        DBG2(printf("trace var is not found"));
    }

    tjv_ValidationElementFree(h->root);

    ckfree(h);

    DBG2(printf("return: ok"));

}
