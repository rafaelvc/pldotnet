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

typedef struct Pldotnet_info
{
    char * libArgs;
    int    typesize_nullflags;
    int    typesize_params;
    int    typesize_result;
}Pldotnet_info;

typedef struct Args_source
{
    char* SourceCode;
    int Result;
    int FuncOid;
}Args_source;

bool Pldotnet_type_supported(Oid type);
const char * Pldotnet_get_dotnet_typename(Oid id, bool hastypeconversion);
int Pldotnet_get_typesize(Oid id);


#endif /* PLDOTNETCOMMON_H */
