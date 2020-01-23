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

/*
 * Directories where C#/F# projects for user code are built when
 * USE_DOTNETBUILD is defined. Otherwise that is where our C#/F# compiler
 * projects are located. Default for Linux is /var/lib/DotNetEngine/
 */
char *root_path = NULL;
char *dnldir = STR(PLNET_ENGINE_DIR);

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

int pldotnet_SetScalarValue(char * argp, Datum datum, FunctionCallInfo fcinfo,
                            int narg, Oid type, bool * nullp)

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

Datum 
pldotnet_GetScalarValue(char * result_ptr, char * resultnull_ptr,
                                             FunctionCallInfo fcinfo, Oid type)
{
    Datum retval = 0;
    VarChar * res_varchar; /* For Unicode/UTF8 support */
    char * str_num;
    char * encoded_str;
    unsigned long * ret_ptr;
    int str_len;

    switch (type)
    {
        case BOOLOID:
            /* Recover flag for null result */
            fcinfo->isnull = *(bool *) (resultnull_ptr);
            if (fcinfo->isnull)
                return (Datum) 0;
            return  BoolGetDatum  ( *(bool *)(result_ptr) );
        case INT4OID:
            fcinfo->isnull = *(bool *) (resultnull_ptr);
            if (fcinfo->isnull)
                return (Datum) 0;
            return Int32GetDatum ( *(int *)(result_ptr) );
        case INT8OID:
            fcinfo->isnull = *(bool *) (resultnull_ptr);
            if (fcinfo->isnull)
                return (Datum) 0;
            return  Int64GetDatum ( *(long *)(result_ptr) );
        case INT2OID:
            fcinfo->isnull = *(bool *) (resultnull_ptr);
            if (fcinfo->isnull)
                return (Datum) 0;
            return  Int16GetDatum ( *(short *)(result_ptr) );
        case FLOAT4OID:
            return  Float4GetDatum ( *(float *)(result_ptr) );
        case FLOAT8OID:
            return  Float8GetDatum ( *(double *)(result_ptr) );
        case NUMERICOID:
            str_num = (char *)*(unsigned long *)(result_ptr);
            return NumericGetDatum(
                                   DirectFunctionCall3(numeric_in,
                                         CStringGetDatum(str_num),
                                         ObjectIdGetDatum(InvalidOid),
                                         Int32GetDatum(-1)));
        case TEXTOID:
             /* C String encoding
              * retval = DirectFunctionCall1(textin,
              *               CStringGetDatum(
              *                       *(unsigned long *)(libargs
              *                       + dotnet_cstruct_info.typesize_params)));
              */
        case BPCHAROID:
 /* https://git.brickabode.com/DotNetInPostgreSQL/pldotnet/issues/10#note_19223
         * We should try to get atttymod which is n size in char(n)
         * and use it in bpcharin (I did not find a way to get it)
         * case BPCHAROID:
         *    retval = DirectFunctionCall1(bpcharin,
         *                           CStringGetDatum(
         *                            *(unsigned long *)(libargs
         *                  + dotnet_cstruct_info.typesize_params)), attypmod);
         */
        case VARCHAROID:
             /* C String encoding
              * retval = DirectFunctionCall1(varcharin,
              *               CStringGetDatum(
              *                       *(unsigned long *)(libargs
              *                       + dotnet_cstruct_info.typesize_params)));
              */
            /* UTF8 encoding */
            ret_ptr = *(unsigned long **)(result_ptr);
            /* str_len = pg_mbstrlen(ret_ptr); */
            str_len = strlen((char*)ret_ptr);
            encoded_str = (char *)pg_do_encoding_conversion( 
            (unsigned char*)ret_ptr, str_len, PG_UTF8, GetDatabaseEncoding() );
            res_varchar = (VarChar *)SPI_palloc(str_len + VARHDRSZ);
#if PG_VERSION_NUM < 80300
            /* Total size of structure, not just data */
            VARATT_SIZEP(res_varchar) = str_len + VARHDRSZ;
#else
            /* Total size of structure, not just data */
            SET_VARSIZE(res_varchar, str_len + VARHDRSZ);
#endif
            memcpy(VARDATA(res_varchar), encoded_str , str_len);
            /* pfree(encoded_str); */
            PG_RETURN_VARCHAR_P(res_varchar);
    }

    return retval;
}

bool
pldotnet_TypeSupported(Oid type)
{
   return (pldotnet_IsSimpleType(type) || pldotnet_IsTextType(type)
           || TYPTYPE_COMPOSITE);
}

bool 
pldotnet_IsSimpleType(Oid type)
{
    return (type == INT2OID || type == INT4OID || type == INT8OID ||
            type == FLOAT4OID || type == FLOAT8OID || type == BOOLOID);
}

bool
pldotnet_IsTextType(Oid type)
{
    /* NUMERIC appears here because it is converted to a CString type */
    return (type == TEXTOID || type == VARCHAROID ||
            type == BPCHAROID || type == NUMERICOID);
}

bool 
pldotnet_IsArray(int narg, pldotnet_FuncInOutInfo * funinout_info)
{
    return (funinout_info->arrayinfo[narg].ixarray == narg);
}

