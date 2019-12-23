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

#include "pldotnet_common.h"
#include "pldotnet_spi.h"

int 
SPIExecute(char* cmd, long limit)
{
    MemoryContext oldcontext;
    ResourceOwner oldowner;
    int rv;

    rv = SPI_execute(cmd, false, limit);
    return SPIFetchResult(SPI_tuptable, rv);
}

int
SPIFetchResult (SPITupleTable *tuptable, int status)
{
    char *key;
    Datum attr_val;
    bool is_null;

    if(status > 0 && tuptable != NULL)
    {
		Form_pg_attribute attr = TupleDescAttr(tuptable->tupdesc, 0);
        key = NameStr(attr->attname);
        attr_val = heap_getattr(tuptable->vals[0],1,tuptable->tupdesc, &is_null);
        return DatumGetInt32(attr_val);
    }

}
