/* 
 * PL/.NET (pldotnet) - PostgreSQL support for .NET C# and F# as 
 *                      procedural languages (PL)
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
 * pldotnet.c - Postgres pldotnet extension init and deinit routines
 *
 */
#include <postgres.h>
#include "pldotnet.h"
#include "pldotnet_common.h"
#include "pldotnet_hostfxr.h"
#include <funcapi.h>
#include <dlfcn.h>

PG_MODULE_MAGIC;

/* Declare extension variables/structs here */
PGDLLEXPORT Datum _PG_init(PG_FUNCTION_ARGS);
PGDLLEXPORT Datum _PG_fini(PG_FUNCTION_ARGS);

#if PG_VERSION_NUM >= 90000
#define CODEBLOCK \
  ((InlineCodeBlock *) DatumGetPointer(PG_GETARG_DATUM(0)))->source_text
PG_FUNCTION_INFO_V1(_PG_init);
Datum _PG_init(PG_FUNCTION_ARGS)
{
    /* Initialize variable structs here
     * Init dotnet runtime here ?
     */
    elog(LOG, "[plldotnet]: _PG_init");

    root_path = strdup(dnldir);
    if (root_path[strlen(root_path) - 1] == DIR_SEPARATOR)
        root_path[strlen(root_path) - 1] = 0;

    PG_RETURN_VOID();
}

PG_FUNCTION_INFO_V1(_PG_fini);
Datum _PG_fini(PG_FUNCTION_ARGS)
{
    /* Deinitialize variable/structs here
     * Close dotnet runtime here ?
     */

    dlclose(nethost_lib);
    PG_RETURN_VOID();
}
#endif
