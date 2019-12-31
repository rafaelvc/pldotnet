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
 * pldotnet_spi.c - Postgres PL handlers for SPI wrappers and functions
 *
 */

#include "pldotnet_spi.h"
#include "pldotnet_hostfxr.h" /* needed for pldotnet_LoadHostfxr() */

int
SPIExecute(char* cmd, long limit)
{
    MemoryContext oldcontext;
    ResourceOwner oldowner;
    int rv;

    rv = SPI_execute(cmd, false, limit);
    SPIFetchResult(SPI_tuptable, rv);
    return 0;
}

int
SPIFetchResult (SPITupleTable *tuptable, int status)
{
    char *key;
    Datum attr_val;
    bool is_null;
    TupleDesc tupdesc = tuptable->tupdesc;
    Form_pg_attribute attr;
    unsigned long *ptr;
    /* delegate related */
    const char csharp_json_path[] = "/src/csharp/DotNetLib.runtimeconfig.json";
    const char csharp_dll_path[] = "/src/csharp/DotNetLib.dll";
    component_entry_point_fn csharp_method = nullptr;
    load_assembly_and_get_function_pointer_fn load_assembly_and_get_function_pointer;
    char dnldir[] = STR(PLNET_ENGINE_DIR);
    char *root_path;
    char *config_path;
    char *dotnetlib_path;
    int  rc;
    
    root_path = strdup(dnldir);
    if (root_path[strlen(root_path) - 1] == DIR_SEPARATOR)
        root_path[strlen(root_path) - 1] = 0;

    /*
     * STEP 1: Load HostFxr and get exported hosting functions
     */
    if (!pldotnet_LoadHostfxr())
        assert(0 && "Failure: pldotnet_LoadHostfxr()");

    /*
     * STEP 2: Initialize and start the .NET Core runtime
     */

    config_path = palloc0(strlen(root_path) + strlen(csharp_json_path) + 1);
    SNPRINTF(config_path, strlen(root_path) + strlen(csharp_json_path) + 1
                    , "%s%s", root_path, csharp_json_path);

    load_assembly_and_get_function_pointer = GetNetLoadAssembly(config_path);
    assert(load_assembly_and_get_function_pointer != nullptr && \
        "Failure: GetNetLoadAssembly()");
    /*
     * STEP 3:
     * Load managed assembly and get function pointer to a managed method
     */
    dotnetlib_path = palloc0(strlen(root_path) + strlen(csharp_dll_path) + 1);
    SNPRINTF(dotnetlib_path,strlen(root_path) + strlen(csharp_dll_path) + 1
                    , "%s%s", root_path, csharp_dll_path);

    elog(WARNING,"\n\n CHECK 5 : %s\n\n", dotnetlib_path);
    rc = load_assembly_and_get_function_pointer(
        dotnetlib_path,
        "DotnNetLib.Lib, DotNetLib",
        "AddProperty",
        nullptr /* delegate_type_name */,
        nullptr,
        (void**)&csharp_method);

    if(status > 0 && tuptable != NULL)
    {
        for (int i = 0; i < tupdesc->natts; i++) 
        {
            elog(WARNING,"\n\n CHECK 6 \n\n");
            csharp_method(&rc, 10);
            //attr = TupleDescAttr(tuptable->tupdesc, i);
            //key = NameStr(attr->attname);
            //attr_val = GetAttributeByNum(tuptable, attr->attnum, &is_null);
            //return DatumGetCString(attr_val);
        }
    }
}
