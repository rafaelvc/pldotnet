/* 
 * PL/.NET (pldotnet) - PostgreSQL support for .NET C# and F# as 
 * 			procedural languages (PL)
 * 
 * 
 * Copyright 2019-2020 Brick Abode
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 * 
 * pldotnet_common.h
 *
 */
#ifndef PLDOTNETCOMMON_H
#define PLDOTNETCOMMON_H

/* PostgreSQL */
#include <postgres.h>
#include <fmgr.h>
#include <funcapi.h>
#include <access/heapam.h>
#include <access/xact.h>
#if PG_VERSION_NUM >= 90300
#include <access/htup_details.h>
#endif
#include <catalog/namespace.h>
#include <catalog/pg_proc.h>
#include <catalog/pg_type.h>
#include <commands/trigger.h>
#include <executor/spi.h>
#include <nodes/makefuncs.h>
#include <parser/parse_type.h>
#include <utils/array.h>
#include <utils/builtins.h>
#include <utils/datum.h>
#include <utils/lsyscache.h>
#include <utils/memutils.h>
#include <utils/rel.h>
#include <utils/syscache.h>
#include <utils/typcache.h>
#include <utils/numeric.h>
#include <mb/pg_wchar.h> /* For UTF8 support */

#include <assert.h>
#include "pldotnet_hostfxr.h"

#include <dlfcn.h>
#include <limits.h>


#if PG_VERSION_NUM < 110000
    #define TupleDescAttr(tupdesc, i) ((tupdesc)->attrs[(i)])
    #define pg_create_context(name) \
            AllocSetContextCreate(TopMemoryContext, \
                name, ALLOCSET_DEFAULT_MINSIZE, ALLOCSET_DEFAULT_INITSIZE, \
                ALLOCSET_DEFAULT_MAXSIZE);
#else
    #define pg_create_context(name) \
            AllocSetContextCreate(TopMemoryContext, name, ALLOCSET_DEFAULT_SIZES);
#endif

/* As a reminder snprintf is defined as pg_snprintf.
 * Check port.h into postgres codebase
 */
#define SNPRINTF(dst, size, fmt, ...)                                    \
    if(snprintf(dst,size,fmt, __VA_ARGS__) >= size){                     \
        elog(ERROR,"[pldotnet] (%s:%d) String too long for buffer: " fmt \
                        ,__FILE__,__LINE__,__VA_ARGS__);                 \
    }

#define QUOTE(name) #name
#define STR(macro) QUOTE(macro)
#define CH(c) c
#define DIR_SEPARATOR '/'
#define MAX_PATH PATH_MAX
/* Null pointer constant definition */
#define nullptr ((void*)0)

typedef struct pldotnet_ArgArrayInfo
{
    int ixarray;
    int typlen;
    bool typbyval;
    char typtype;
    Oid typelem;
    char typalign;
    int ndim;
    const int * dims;
    int nelems;
    char csharpdecl[64];
}pldotnet_ArgArrayInfo;

typedef struct pldotnet_FuncInOutInfo
{
    int typesize_nullflags;
    int typesize_args;
    int typesize_result;
    pldotnet_ArgArrayInfo arrayinfo[64]; /* check max nr of args */
}pldotnet_FuncInOutInfo;

typedef struct pldotnet_ArgsSource
{
    char* source_code;
    int result;
    int func_oid;
}pldotnet_ArgsSource;

bool pldotnet_TypeSupported(Oid type);
const char * pldotnet_GetNetTypeName(Oid id, bool hastypeconversion);
int pldotnet_GetTypeSize(Oid id);
const char * pldotnet_GetUnmanagedTypeName(Oid type);
int pldotnet_SetScalarValue(char * argp, Datum datum, FunctionCallInfo fcinfo,
                            int narg, Oid type, bool * nullp);
Datum pldotnet_GetScalarValue(char * result_ptr, char * resultnull_ptr,
                              FunctionCallInfo fcinfo, Oid type);
bool pldotnet_IsArray(int narg, pldotnet_FuncInOutInfo * funinout_info);
bool pldotnet_IsSimpleType(Oid type);
bool pldotnet_IsTextType(Oid type);

/*
 * Directories where C#/F# projects for user code are built when
 * USE_DOTNETBUILD is defined. Otherwise that is where our C#/F# compiler
 * projects are located. Default for Linux is /var/lib/DotNetEngine/
 */
char *root_path;
char *dnldir;

#endif /* PLDOTNETCOMMON_H */
