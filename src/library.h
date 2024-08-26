/**
 * Copyright Jerily LTD. All Rights Reserved.
 * SPDX-FileCopyrightText: 2024 Neofytos Dimitriou (neo@jerily.cy)
 * SPDX-License-Identifier: MIT.
 */
#ifndef TJV_LIBRARY_H
#define TJV_LIBRARY_H

#include "common.h"
#include "tjvCompile.h"

typedef struct {
    Tcl_Interp *interp;
    Tcl_Command cmd_token;
    Tcl_Obj *cmd_name;
    Tcl_Obj *trace_var;
    tjv_ValidationElement *root;
} tjv_ValidationHandler;

static Tcl_Config const tjv_pkgconfig[] = {
    { "package-version", XSTR(VERSION) },
    {NULL, NULL}
};

#ifdef __cplusplus
extern "C" {
#endif

EXTERN int Tjv_Init(Tcl_Interp *interp);
#ifdef USE_NAVISERVER
NS_EXTERN int Ns_ModuleVersion = 1;
NS_EXTERN int Ns_ModuleInit(const char *server, const char *module);
#endif

#ifdef __cplusplus
}
#endif

#endif // TJV_LIBRARY_H
