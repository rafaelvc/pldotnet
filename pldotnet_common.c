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
 * pldotnet_common.c - Common functions for PG <-> .NET type delivering
 *
 */
#include "pldotnet_common.h"

/*
 * Directories where C#/F# projects for user code are built when
 * USE_DOTNETBUILD is defined. Otherwise that is where our C#/F# compiler
 * projects are located. Default for Linux is /var/lib/DotNetEngine/
 */
char *root_path = NULL;
char *dnldir = STR(PLNET_ENGINE_DIR);

bool
pldotnet_TypeSupported(Oid type)
{
    return (type == INT4OID || type == INT8OID
       || type == INT2OID   || type == FLOAT4OID
       || type == FLOAT8OID || type == VARCHAROID
       || type == BOOLOID   || type == TEXTOID
       || type == BPCHAROID || type == NUMERICOID);
}

const char *
pldotnet_GetNetTypeName(Oid id, bool hastypeconversion)
{
    switch (id)
    {
        case BOOLOID:
            return "bool";   /* System.Boolean */
        case INT4OID:
            return "int";    /* System.Int32 */
        case INT8OID:
            return "long";   /* System.Int64 */
        case INT2OID:
            return "short";  /* System.Int16 */
        case FLOAT4OID:
            return "float";  /* System.Single */
        case FLOAT8OID:
            return "double"; /* System.Double */
        case NUMERICOID:     /* System.Decimal */
            return hastypeconversion ? "string" : "decimal";
        case BPCHAROID:
        case TEXTOID:
        case VARCHAROID:
            return "string"; /* System.String */
    }
    return "";
}

/* Native type size in bytes */
int
pldotnet_GetTypeSize(Oid id)
{
    switch (id)
    {
        case BOOLOID:
            return sizeof(bool);
        case INT4OID:
            return sizeof(int);
        case INT8OID:
            return sizeof(long);
        case INT2OID:
            return sizeof(short);
        case FLOAT4OID:
            return sizeof(float);
        case FLOAT8OID:
            return sizeof(double);
        case NUMERICOID:
        case BPCHAROID:
        case TEXTOID:
        case VARCHAROID:
            return sizeof(char *);
    }
    return -1;
}

const char * 
pldotnet_GetUnmanagedTypeName(Oid type)
{
    switch (type)
    {
        case BOOLOID:
            return "U1";
        case INT2OID:
            return "I2";
        case INT4OID:
            return "I4";
        case INT8OID:
            return "I8";
        case FLOAT4OID:
            return "R4";
        case FLOAT8OID:
            return "R8";
        case NUMERICOID:
            return "LPStr";
        case BPCHAROID:
        case VARCHAROID:
        case TEXTOID:
            /*return "LPUTF8Str";  review why marshal is not working */
            return "LPStr";
    }
    return  "";
}

int pldotnet_SetScalarValue(char * argp, Datum datum,
                      pldotnet_FuncInOutInfo * funinout_info,
                      FunctionCallInfo fcinfo, int narg, Oid type, bool * nullp)
{
    char * newstr;
    int len;
    bool isnull = false;

#if PG_VERSION_NUM >= 120000
    if (nullp)
        isnull=datum.isnull;
#else
    if (nullp)
        isnull=fcinfo->argnull[narg];
#endif

    switch (type)
    {
        case BOOLOID:
            *(bool *)(argp) = DatumGetBool(datum);
            if (nullp)
                *nullp = isnull;
            break;
        case INT4OID:
            *(int *)(argp) = DatumGetInt32(datum);
            if (nullp)
                *nullp = isnull;
            break;
        case INT8OID:
            *(long *)(argp) = DatumGetInt64(datum);
            if (nullp)
                *nullp = isnull;
            break;
        case INT2OID:
            *(short *)(argp) = DatumGetInt16(datum);
            if (argp)
                *nullp = isnull;
            break;
        case FLOAT4OID:
            *(float *)(argp) = DatumGetFloat4(datum);
            break;
        case FLOAT8OID:
            *(double *)(argp) = DatumGetFloat8(datum);
            break;
        case NUMERICOID:
            /* C String encoding (numeric_out) is used here as it
             is a number. Unlikely to have encoding issues. */
            *(unsigned long *)(argp) = (unsigned long)
                DatumGetCString(DirectFunctionCall1(numeric_out, datum));
            break;
        case BPCHAROID:
        case TEXTOID:
        case VARCHAROID:

           /* UTF8 encoding */
           len = VARSIZE( DatumGetTextP (datum) ) - VARHDRSZ;
           newstr = (char *)palloc0(len+1);
           memcpy(newstr, VARDATA( DatumGetTextP(datum) ), len);
           *(unsigned long *)(argp) = (unsigned long)
                    pg_do_encoding_conversion((unsigned char *)newstr,
                                              len+1,
                                              GetDatabaseEncoding(), PG_UTF8);

            /*  If you need C String encoding do like this:
                *(unsigned long *)argp =
                DirectFunctionCall1(cstrfunc, DatumGetCString(datum));
                where cstrfunc is bpcharout, textout or varcharout */
            break;
    }
    return 0;
}

bool 
pldotnet_IsSimpleType(Oid type)
{
    return (type == INT2OID || type == INT4OID || type == INT8OID ||
            type == FLOAT4OID || type == FLOAT8OID || type == BOOLOID);
}

bool 
pldotnet_IsArray(int narg, pldotnet_FuncInOutInfo * funinout_info)
{
    return (funinout_info->arrayinfo[narg].ixarray == narg);
}

