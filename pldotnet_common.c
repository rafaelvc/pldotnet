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
#include "pldotnet_composites.h"

bool
pldotnet_TypeSupported(Oid type)
{
    return (pldotnet_IsSimpleType(type) || TYPTYPE_COMPOSITE);
}

bool
pldotnet_IsSimpleType(Oid type)
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
    Form_pg_type typeinfo;
    HeapTuple typ;
    char * composite_nm;

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
        default:
            typ = SearchSysCache(TYPEOID, 
                                  ObjectIdGetDatum(id), 0, 0, 0);
            if (!HeapTupleIsValid(typ)) 
            {
                elog(ERROR, "[pldotnet]: cache lookup failed for type %u", 
                                                                         id);
            }
            typeinfo = (Form_pg_type) GETSTRUCT(typ);
            if (typeinfo->typtype == TYPTYPE_COMPOSITE)
            {
                composite_nm = NameStr(typeinfo->typname);
                ReleaseSysCache(typ);
                return composite_nm;
            }
            ReleaseSysCache(typ);
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
        default:
            return pldotnet_GetCompositeTypeSize(id);
    }
    return -1;
}

int pldotnet_SetScalarValue(char * argp, Datum datum,
                      FunctionCallInfo fcinfo, int narg, Oid type, bool * nullp)
{
    char * newstr;
    int len;
    bool isnull = false;
    //bool isarr = pldotnet_IsArray(narg);
    bool isarr = false;

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
            *(bool *)(argp) = DatumGetBool(isarr ? *(Datum *)datum: datum);
            if (nullp)
                *nullp = isnull;
            break;
        case INT4OID:
            *(int *)(argp) = DatumGetInt32(isarr ? *(Datum *)datum: datum);
            if (nullp)
                *nullp = isnull;
            break;
        case INT8OID:
            *(long *)(argp) = DatumGetInt64(isarr ? *(Datum *)datum: datum);
            if (nullp)
                *nullp = isnull;
            break;
        case INT2OID:
            *(short *)(argp) = DatumGetInt16(isarr ? *(Datum *)datum: datum);
            if (argp)
                *nullp = isnull;
            break;
        case FLOAT4OID:
            *(float *)(argp) = DatumGetFloat4(isarr ? *(Datum *)datum: datum);
            break;
        case FLOAT8OID:
            *(double *)(argp) = DatumGetFloat8(isarr ? *(Datum *)datum: datum);
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


