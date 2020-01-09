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

extern load_assembly_and_get_function_pointer_fn load_assembly_and_get_function_pointer;

int
SPIExecute(char* cmd, long limit)
{
    //MemoryContext oldcontext;
    //ResourceOwner oldowner;
    int rv;

    rv = SPI_execute(cmd, false, limit);
    SPIFetchResult(SPI_tuptable, rv);
    return 0;
}

int
SPIFetchResult (SPITupleTable *tuptable, int status)
{
    Datum attr_val;
    bool is_null;
    TupleDesc tupdesc = tuptable->tupdesc;
    Form_pg_attribute attr;
    /* delegate related */
    const char csharp_json_path[] = "/src/csharp/DotNetLib.runtimeconfig.json";
    const char csharp_dll_path[] = "/src/csharp/DotNetLib.dll";
    component_entry_point_fn csharp_method = nullptr;
    char dnldir[] = STR(PLNET_ENGINE_DIR);
    char *root_path;
    char *config_path;
    char *dotnetlib_path;
    int  rc;
    PropertyValue val;
    int num_row;

    root_path = strdup(dnldir);
    if (root_path[strlen(root_path) - 1] == DIR_SEPARATOR)
        root_path[strlen(root_path) - 1] = 0;

    config_path = palloc0(strlen(root_path) + strlen(csharp_json_path) + 1);
    SNPRINTF(config_path, strlen(root_path) + strlen(csharp_json_path) + 1
                    , "%s%s", root_path, csharp_json_path);

    dotnetlib_path = palloc0(strlen(root_path) + strlen(csharp_dll_path) + 1);
    SNPRINTF(dotnetlib_path,strlen(root_path) + strlen(csharp_dll_path) + 1
                    , "%s%s", root_path, csharp_dll_path);

    rc = load_assembly_and_get_function_pointer(
        dotnetlib_path,
        "DotNetLib.Lib, DotNetLib",
        "AddProperty",
        nullptr /* delegate_type_name */,
        nullptr,
        (void**)&csharp_method);

    if(status > 0 && tuptable != NULL)
    {
        for (num_row = 0; num_row < SPI_processed; num_row++)
        {
            for (int i = 0; i < tupdesc->natts; i++) 
            {
                attr = TupleDescAttr(tuptable->tupdesc, i);
                val.name = NameStr(attr->attname);
                val.type = attr->atttypid;

                if(strcmp(val.name,"?column?") == 0)
                    val.name = "column";

                val.nrow = num_row;
                attr_val = heap_getattr(
                               tuptable->vals[num_row],
                               attr->attnum,
                               tuptable->tupdesc,
                               &is_null);

                val.value = &attr_val;
                csharp_method(&val, sizeof(PropertyValue));
            }
        }
    }

    return 1;
}
