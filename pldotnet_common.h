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

#define QUOTE(name) #name
#define STR(macro) QUOTE(macro)
#define CH(c) c
#define DIR_SEPARATOR '/'
#define MAX_PATH PATH_MAX
// Null pointer constant definition
#define nullptr ((void*)0)

typedef struct pldotnet_info
{
    char * libArgs;
    int    typeSizeNullFlags;
    int    typeSizeOfParams;
    int    typeSizeOfResult;
}pldotnet_info;

typedef struct args_source
{
    char* SourceCode;
    int Result;
    int FuncOid;
}args_source;

bool pldotnet_type_supported(Oid type);
const char * pldotnet_getNetTypeName(Oid id, bool hasTypeConversion);
int pldotnet_getTypeSize(Oid id);


#endif // PLDOTNETCOMMON_H
