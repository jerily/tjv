/**
 * Copyright Jerily LTD. All Rights Reserved.
 * SPDX-FileCopyrightText: 2024 Neofytos Dimitriou (neo@jerily.cy)
 * SPDX-License-Identifier: MIT.
 */

#include "library.h"

static Tcl_VarTraceProc tjv_HandleVarTraceProc;
static Tcl_CmdDeleteProc tjv_HandleDeleteProc;

static int tjv_HandleCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]) {

    DBG2(printf("enter: objc: %d", objc));

    UNUSED(clientData);
    UNUSED(interp);
    UNUSED(objc);
    UNUSED(objv);

    DBG2(printf("return: ok"));

    return TCL_OK;

}

static int tjv_CompileCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]) {

    DBG2(printf("enter: objc: %d", objc));

    UNUSED(clientData);
    CheckArgs(2, 3, 1, "validation_scheme ?variable_name?");

    tjv_ValidationElement *root = tjv_ValidationCompileFromObj(interp, objv[1]);
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

    if (objc > 2) {
        h->trace_var = objv[2];
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

    Tcl_CreateNamespace(interp, "::tjv", NULL, NULL);
    Tcl_CreateObjCommand(interp, "::tjv::compile", tjv_CompileCmd, NULL, NULL);


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

    DBG2(printf("return: ok"));

}
