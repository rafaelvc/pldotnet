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
#include <mb/pg_wchar.h> /* For UTF8 support */

extern load_assembly_and_get_function_pointer_fn
load_assembly_and_get_function_pointer;

int
pldotnet_SPIExecute(char* cmd, long limit)
{
    int rv;

    PG_TRY();
    {
        rv = SPI_execute(cmd, false, limit);
        pldotnet_SPIFetchResult(SPI_tuptable, rv);
    }
    PG_CATCH();
    {
        /* Do the excption handling */
        elog(WARNING, "Exception");
        PG_RE_THROW();
    }
    PG_END_TRY();

    return 0;
}

int
pldotnet_SPIFetchResult (SPITupleTable *tuptable, int status)
{
    Datum attr_val;
    bool is_null;
    TupleDesc tupdesc = tuptable->tupdesc;
    Form_pg_attribute attr;
    /* delegate related */
    char dotnet_type[] = "PlDotNET.Engine, PlDotNET";
    char dotnet_type_method[64] = "InvokeAddProperty";
    /* Auxiliary vars for obtaning the correct pointer
     * TODO: Remove it for a generic one, maybe union?
     */
    bool bool_aux;
    float float_aux;
    double double_aux;
    int buff_len;
    char *newargvl;
    /**/
    int  rc;
    PropertyValue val;
    int num_row;

    if(status > 0 && tuptable != NULL)
    {
        for (num_row = 0; num_row < SPI_processed; num_row++)
        {
            for (int i = 0; i < tupdesc->natts; i++)
            {
                attr = TupleDescAttr(tuptable->tupdesc, i);
                val.name = NameStr(attr->attname);
                val.type = attr->atttypid;
                val.nrow = num_row;

                /* Edge cases for special character
                 * in column name from SELECT X command */
                if(strcmp(val.name,"?column?") == 0 ||
                   strcmp(val.name,"bool") == 0
                )
                    val.name = "column";

                attr_val = heap_getattr(
                               tuptable->vals[num_row],
                               attr->attnum,
                               tuptable->tupdesc,
                               &is_null);

                switch (val.type)
                {
                    case INT2OID:
                    case INT4OID:
                    case INT8OID:
                        val.value = (Datum) &attr_val;
                        break;
                    case BOOLOID:
                        bool_aux = DatumGetBool(attr_val);
                        val.value = (Datum) &bool_aux;
                        break;
                    case FLOAT4OID:
                        float_aux = DatumGetFloat4(attr_val);
                        val.value = (Datum) &float_aux;
                        break;
                    case FLOAT8OID:
                        double_aux = DatumGetFloat8(attr_val);
                        val.value = (Datum) &double_aux;
                        break;
                    case NUMERICOID:
                        val.value = (Datum) DatumGetCString(
                                        DirectFunctionCall1(numeric_out,
                                                            attr_val));
                        break;
                    case VARCHAROID:
                        buff_len = VARSIZE(DatumGetTextP(attr_val)) - VARHDRSZ;
                        newargvl = (char *)palloc0(buff_len + 1);
                        memcpy(newargvl, VARDATA(DatumGetTextP(attr_val)),
                                                 buff_len);
                        val.value = (Datum)
                             pg_do_encoding_conversion((unsigned char*)newargvl,
                                                       buff_len+1,
                                                       GetDatabaseEncoding(),
                                                       PG_UTF8);
                        break;
                }
                plcsharp_Run(dotnet_type,
                             dotnet_type_method,
                             (char *)&val,
                             sizeof(PropertyValue));
            }
        }
    }

    return 0;
}
