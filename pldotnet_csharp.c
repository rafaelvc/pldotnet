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
 * pldotnet_csharp.c - Postgres PL handlers for C# and functions
 *
 */
#include "pldotnet_csharp.h"
#include <math.h>
#include <mb/pg_wchar.h> /* For UTF8 support */
#include <utils/numeric.h>

static pldotnet_CStructInfo dotnet_cstruct_info;

/* Declare extension variables/structs here */
PGDLLEXPORT Datum plcsharp_call_handler(PG_FUNCTION_ARGS);
PGDLLEXPORT Datum plcsharp_validator(PG_FUNCTION_ARGS);
#if PG_VERSION_NUM >= 90000
PGDLLEXPORT Datum plcsharp_inline_handler(PG_FUNCTION_ARGS);
#endif

static char  *plcsharp_BuildBlock2(Form_pg_proc procst);
static char  *plcsharp_BuildBlock4(Form_pg_proc procst);
static char  *plcsharp_BuildBlock5(Form_pg_proc procst, HeapTuple proc);
static int   GetSizeNullableHeader(int argnm_size, Oid arg_type, int narg);
static int   GetSizeNullableFooter(Oid ret_type);
static bool  IsNullable(Oid type);
static char  *pldotnet_CreateCStructLibargs(FunctionCallInfo fcinfo, Form_pg_proc procst);
static Datum pldotnet_GetNetResult(char * libargs, Oid rettype, FunctionCallInfo fcinfo);
static int   GetSizeArgsNullArray(int nargs);
static int   pldotnet_PublicDeclSize(Oid type);
static char  *pldotnet_PublicDecl(Oid type);
static const char * pldotnet_GetNullableTypeName(Oid id);

#if PG_VERSION_NUM >= 90000
#define CODEBLOCK \
  ((InlineCodeBlock *) DatumGetPointer(PG_GETARG_DATUM(0)))->source_text

const char public_bool[] = "\n[MarshalAs(UnmanagedType.U1)]public ";
const char public_string_utf8[] = "\n[MarshalAs(UnmanagedType.LPUTF8Str)]public ";
const char public_[] = "\npublic ";
/* nullable related constants */
const char resu_nullable_value[] = "libargs.resu = resu_nullable.GetValueOrDefault();\n";
const char resu_nullable_flag[] = "libargs.resunull = !resu_nullable.HasValue;\n";
const char argsnull_str[] = "libargs.argsnull";
const char nullable_suffix[] = "_nullable";
const char resu_flag_str[] = "bool resunull;";
const char arg_flag_str[] = "bool[] argsnull;";

/* C# CODE TEMPLATE */
static char cs_block_call1[] = "            \n\
using System;                               \n\
using System.Runtime.InteropServices;       \n\
namespace DotNetLib                         \n\
{                                           \n\
    public static class ProcedureClass      \n\
    {                                       \n\
        [StructLayout(LayoutKind.Sequential,Pack=1)]\n\
        public struct LibArgs                \n\
        {";
/*********** cs_block_call2 **********
 *          public argType1 argname1;
 *          public argType2 argname2;
 *           ...
 *	        public returnT resu;
 */
static char cs_block_call3[] = "             \n\
        }                                    \n\
        public static int ProcedureMethod(IntPtr arg, int argLength)\n\
        {                                    \n\
            if (argLength != System.Runtime.InteropServices.Marshal.SizeOf(typeof(LibArgs)))\n\
                return 1;                    \n\
            LibArgs libargs = Marshal.PtrToStructure<LibArgs>(arg);\n";
/*********** cs_block_call4 **********
 *          libargs.resu = FUNC(libargs.argname1, libargs.argname2, ...);
 */

/*********** cs_block_call5 **********
 *          returnT FUNC(argType1 argname1, argType2 argname2, ...)
 *          {
 *               What is in the SQL function code here
 *          }
 */
static char cs_block_call6[] = "              \n\
            Marshal.StructureToPtr<LibArgs>(libargs, arg, false);\n\
            return 0;                         \n\
        }                                     \n\
    }                                         \n\
}";


static char block_inline1[] = "             \n\
using System;                               \n\
using System.Runtime.InteropServices;       \n\
namespace DotNetLib                       \n\
{                                           \n\
    public static class ProcedureClass      \n\
    {";
static char block_inline2[] = "                    \n\
        public static int ProcedureMethod(IntPtr arg, int argLength)\n\
        {";                                   
/* block_inline3   Function body */
static char block_inline4[] = "              \n\
	    return 0; \n\
	}                                   \n\
     }                                      \n\
}";

static int
pldotnet_PublicDeclSize(Oid type)
{
    switch (type)
    {
        case BOOLOID:
            return strlen(public_bool);
        case BPCHAROID:
        case VARCHAROID:
        case TEXTOID:
            return strlen(public_string_utf8);
        default:
            return strlen(public_);
    }
    return  0;
}

static char *
pldotnet_PublicDecl(Oid type)
{
    switch (type)
    {
        case BOOLOID:
            return (char *)&public_bool;
        case BPCHAROID:
        case VARCHAROID:
        case TEXTOID:
            return (char *)&public_string_utf8;
        default:
            return (char *)&public_;
    }
    return  0;
}

static char *
plcsharp_BuildBlock2(Form_pg_proc procst)
{
    char *block2str, *str_ptr;
    Oid *argtype = procst->proargtypes.values; /* Indicates the args type */
    Oid rettype = procst->prorettype; /* Indicates the return type */
    int nargs = procst->pronargs;
    const char semicon[] = ";";
    char argname[] = " argN";
    char result[] = " resu"; /*  have to be same size argN */
    int i, cursize = 0, totalsize = 0, public_size;
    /* nullable related */
    bool nullable_arg_flag = false;
    int null_flags_size = 0, return_null_flag_size = 0;

    if (!pldotnet_TypeSupported(rettype))
    {
        elog(ERROR, "[pldotnet]: unsupported type on return");
        return 0;
    }

    for (i = 0; i < nargs; i++)
    {
        if (!pldotnet_TypeSupported(argtype[i]))
        {
            elog(ERROR, "[pldotnet]: unsupported type on arg %d", i);
            return 0;
        }

        if (IsNullable(argtype[i]))
            nullable_arg_flag = true;

        public_size = pldotnet_PublicDeclSize(argtype[i]);
        totalsize += public_size + strlen(pldotnet_GetNetTypeName(argtype[i], true)) +
                       + strlen(argname) + strlen(semicon);
    }
    
    public_size = pldotnet_PublicDeclSize(rettype);

    return_null_flag_size = strlen(public_bool) + strlen(resu_flag_str);

    if (nullable_arg_flag)
        null_flags_size = GetSizeArgsNullArray(nargs);

    totalsize += public_size 
                    + strlen(pldotnet_GetNetTypeName(rettype, true))
                    + null_flags_size + return_null_flag_size
                    + strlen(result) + strlen(semicon) + 1;

    block2str = (char *) palloc0(totalsize);

    if (nullable_arg_flag)
    {
        str_ptr = (char *)(block2str + cursize);
        SNPRINTF(str_ptr, totalsize - cursize
            , "\n[MarshalAs(UnmanagedType.ByValArray,ArraySubType=UnmanagedType.U1,SizeConst=%d)]public %s"
            , nargs, arg_flag_str);
        cursize += strlen(str_ptr);
    }

    str_ptr = (char *)(block2str + cursize);
    SNPRINTF(str_ptr,totalsize - cursize,"%s%s",public_bool,resu_flag_str);
    cursize += strlen(str_ptr);

    for (i = 0; i < nargs; i++)
    {
        SNPRINTF(argname,strlen(argname)+1, " arg%d", i); /* review nargs > 9 */
        str_ptr = (char *)(block2str + cursize);
        SNPRINTF(str_ptr,totalsize - cursize,"%s%s%s%s"
                    , pldotnet_PublicDecl(argtype[i])
                    , pldotnet_GetNetTypeName(argtype[i], true)
                    , argname, semicon);
        cursize += strlen(str_ptr);
    }

    /* result */
    str_ptr = (char *)(block2str + cursize);


    SNPRINTF(str_ptr,totalsize - cursize,"%s%s%s%s"
                ,pldotnet_PublicDecl(rettype)
                ,pldotnet_GetNetTypeName(rettype, true), result, semicon);

    return block2str;
}

static char *
plcsharp_BuildBlock4(Form_pg_proc procst)
{
    char *block2str, *str_ptr, *resu_var;
    int cursize = 0, i, totalsize;
    const char beginFun[] = "(";
    char * func;
    const char libargs[] = "libargs.";
    const char strconvert[] = ".ToString()"; /* Converts func return */
    const char todecimal[] = "Convert.ToDecimal(";
    const char comma[] = ",";
    char argname[] = "argN";
    const char end_fun[] = ")";
    const char semicolon[] = ";";
    int nargs = procst->pronargs;
    Oid *argtype = procst->proargtypes.values; /* Indicates the args type */
    Oid rettype = procst->prorettype; /* Indicates the return type */

    /* Function name */
    func = NameStr(procst->proname);

    if (IsNullable(rettype))
    {
        const char nullable_result[] = "resu_nullable=";
        int resu_var_size = strlen(pldotnet_GetNullableTypeName(rettype)) 
                + strlen(nullable_result) + 1;

        resu_var = (char *)palloc0(resu_var_size);
        SNPRINTF(resu_var, resu_var_size, "%s%s"
                   , pldotnet_GetNullableTypeName(rettype)
                   , nullable_result);
    } 
    else
    {
        const char result[] = "libargs.resu=";
        resu_var = (char *)palloc0(strlen(result)+1);
        SNPRINTF(resu_var,strlen(result)+1,"%s",result);
    }

    /* TODO:  review for nargs > 9 */
    if (nargs == 0)
    {
         int block_size;

         if (rettype == NUMERICOID)
         {
            block_size = strlen(resu_var) + strlen(func) + strlen(beginFun)
                                 + strlen(end_fun) + strlen(strconvert)
                                 + strlen(semicolon) + 1;
            block2str = (char *)palloc0(block_size);
            SNPRINTF(block2str,block_size,"%s%s%s%s%s%s"
                       , resu_var, func, beginFun
                       , end_fun, strconvert, semicolon);
         }
         else
         {
            block_size = strlen(resu_var) + strlen(func) + strlen(beginFun)
                                 + strlen(end_fun) + strlen(semicolon) + 1;
            block2str = (char *)palloc0(block_size);
            SNPRINTF(block2str,block_size,"%s%s%s%s%s"
                        ,resu_var, func, beginFun, end_fun, semicolon);
         }
         return block2str;
    }

    totalsize = strlen(resu_var) + strlen(func) + strlen(beginFun) +
                    (strlen(libargs) + strlen(argname)) * nargs
                     + strlen(end_fun) + strlen(semicolon) + 1;

    for (i = 0; i < nargs; i++) /* Get number of Numeric argr */
    {
        if (argtype[i] == NUMERICOID)
            totalsize += strlen(todecimal) + strlen(end_fun);
    }

    if (rettype == NUMERICOID)
        totalsize += strlen(strconvert);

    if (nargs > 1)
         totalsize += (nargs - 1) * strlen(comma);

    block2str = (char *) palloc0(totalsize);
    SNPRINTF(block2str, totalsize - cursize, "%s%s%s", resu_var, func, beginFun);
    cursize = strlen(resu_var) + strlen(func) + strlen(beginFun);

    for (i = 0; i < nargs; i++)
    {
        SNPRINTF(argname,strlen(argname)+1, "arg%d", i); /* review nargs > 9 */
        str_ptr = (char *)(block2str + cursize);
        if (i + 1 == nargs)  /* last no comma */
        {
            if (argtype[i] == NUMERICOID)
            {
                SNPRINTF(str_ptr,totalsize-cursize,"%s%s%s%s"
                            , todecimal, libargs, argname, end_fun);
            }
            else
            {
                SNPRINTF(str_ptr, totalsize-cursize, "%s%s", libargs, argname);
            }
        }
        else
        {
            if (argtype[i] == NUMERICOID)
            {
                SNPRINTF(str_ptr, totalsize-cursize, "%s%s%s%s%s"
                            , todecimal, libargs, argname, end_fun, comma);
            }
            else
            {
                SNPRINTF(str_ptr, totalsize-cursize, "%s%s%s"
                            , libargs, argname, comma);
            }
        }
        cursize = strlen(block2str);
    }

    str_ptr = (char *)(block2str + cursize);
    if (rettype == NUMERICOID)
    {
        SNPRINTF(str_ptr, totalsize-cursize, "%s%s%s"
                   , end_fun, strconvert, semicolon);
    }
    else
    {
        SNPRINTF(str_ptr, totalsize-cursize, "%s%s", end_fun, semicolon);
    }

    return block2str;

}

static int
GetSizeArgsNullArray(int nargs)
{
    const char public_bool_array[] =
        "\n[MarshalAs(UnmanagedType.ByValArray,ArraySubType=UnmanagedType.U1,SizeConst=)]public ";
    int n_digits_args = 0;

    if (nargs > 0)
        n_digits_args = floor(log10(abs(nargs))) + 1;

    return (strlen(public_bool_array) + n_digits_args + strlen(arg_flag_str));
}

/*
 * Returns the size of typical C# line converting
 * a struct argument to a nullable C# type argument
 */
static int
GetSizeNullableHeader(int argnm_size, Oid arg_type, int narg)
{
    int total_size = 0;
    char *question_mark = "?";
    char *equal_char = "=";
    char *parenthesis_char = "("; /* same for ')' */
    char *square_bracket_char = "["; /* same for ']' */
    char *colon_char = ":";
    char *semicolon_char = ";";
    char *newline_char = "\n";
    char *null_str = "null";
    int n_digits_arg = 0;

    if (narg == 0)
        /* Edge case treatment since log10(0) == -HUGE_VAL */
        n_digits_arg = floor(log10(abs(1))) + 1;
    else
        n_digits_arg = floor(log10(abs(narg))) + 1;

    switch (arg_type)
    {
        case INT2OID:
        case INT4OID:
        case INT8OID:
        case BOOLOID:
            /* template: 
             * bool? <arg>=argsnull[i]? (bool?)null : <arg>_nullable; */
            total_size = strlen(pldotnet_GetNullableTypeName(arg_type))
                + argnm_size + strlen(equal_char) + strlen(argsnull_str)
                + strlen(square_bracket_char) + n_digits_arg +
                + strlen(square_bracket_char) + strlen(question_mark)
                + strlen(parenthesis_char)
                + strlen(pldotnet_GetNullableTypeName(arg_type))
                + strlen(parenthesis_char) + strlen(null_str)
                + strlen(colon_char) + argnm_size + strlen(nullable_suffix)
                + strlen(semicolon_char) + strlen(newline_char);
            break;
    }

    return total_size;
}

/*
 * Returns the size of typical C# line converting
 * a nullable C# type return to a struct return
 */
static int
GetSizeNullableFooter(Oid ret_type)
{
    int total_size = 0;

    switch (ret_type)
    {
        case INT2OID:
        case INT4OID:
        case INT8OID:
        case BOOLOID:
            total_size = strlen(resu_nullable_value)
                + strlen(resu_nullable_flag);
            break;
    }

    return total_size;
}

static bool
IsNullable(Oid type)
{
    return (type == INT2OID || type == INT4OID
       || type == INT8OID   ||  type == BOOLOID);
}

static char *
plcsharp_BuildBlock5(Form_pg_proc procst, HeapTuple proc)
{
    char *block2str, *str_ptr, *argnm, *source_argnm, *source_text;
    int argnm_size, i, nnames, cursize = 0, source_size, totalsize;
    bool isnull;
    const char begin_fun_decl[] = "(";
    char * func;
    const char comma[] = ",";
    const char end_fun_decl[] = "){\n\n";
    const char end_fun[] = "}\n";
    const char newline[] = "\n";
    int nargs = procst->pronargs;
    Oid rettype = procst->prorettype;
    Datum *argname, argnames, prosrc;
    text * t;
    Oid *argtype = procst->proargtypes.values; /* Indicates the args type */
    /* nullable related */
    char *header_nullable, *header_nullable_ptr;
    int header_size=0, cur_header_size, footer_size=0;

    /* Function name */
    func = NameStr(procst->proname);

    /* Source code */
    prosrc = SysCacheGetAttr(PROCOID, proc, Anum_pg_proc_prosrc, &isnull);
    t = DatumGetTextP(prosrc);
    source_text = DirectFunctionCall1(textout, DatumGetCString(t));
    source_size = strlen(source_text);

    argnames = SysCacheGetAttr(PROCOID, proc,
        Anum_pg_proc_proargnames, &isnull);

    if (!isnull)
      deconstruct_array(DatumGetArrayTypeP(argnames), TEXTOID, -1, false,
          'i', &argname, NULL, &nnames);

    /* Caculates the total amount in bytes of C# src text for 
     * the function declaration according nr of arguments 
     * their types and the function return type
     */
    if (IsNullable(rettype))
    {
        totalsize = (2 * strlen(newline))
            + strlen(pldotnet_GetNullableTypeName(rettype))
            + strlen(" ") + strlen(func) + strlen(begin_fun_decl);
    }
    else
    {
        totalsize = (2 * strlen(newline))
            + strlen(pldotnet_GetNetTypeName(rettype, false))
            + strlen(" ") + strlen(func) + strlen(begin_fun_decl);
    }

    for (i = 0; i < nargs; i++) 
    {
        source_argnm = DirectFunctionCall1(textout,
                DatumGetCString(DatumGetTextP(argname[i])) );

        if (IsNullable(argtype[i]))
        {
            header_size += GetSizeNullableHeader(strlen(source_argnm),argtype[i],i);
            argnm = palloc0(strlen(source_argnm) + strlen("_nullable") + 1);
            SNPRINTF(argnm,strlen(source_argnm) + strlen("_nullable") + 1
                            , "%s_nullable", source_argnm);
        } 
        else
        {
            argnm = palloc0(strlen(source_argnm) + 1);
            SNPRINTF(argnm,strlen(source_argnm) + 1, "%s", source_argnm);
        }

        argnm_size = strlen(argnm);
        /* +1 here is the space between type" "argname declaration */
        totalsize +=  strlen(pldotnet_GetNetTypeName(argtype[i], false))
            + 1 + argnm_size;
        /* cleaning up for next palloc */
        bzero(argnm,sizeof(argnm));
    }
     if (nargs > 1)
         totalsize += (nargs - 1) * strlen(comma); /* commas size */

    footer_size = GetSizeNullableFooter(rettype);

    totalsize += strlen(end_fun_decl) + header_size + source_size
        + strlen(end_fun) + footer_size + 1;

    block2str = (char *)palloc0(totalsize);

    if (IsNullable(rettype))
    {
        SNPRINTF(block2str, totalsize - cursize, "%s%s%s %s%s",newline
                   , newline, pldotnet_GetNullableTypeName(rettype)
                   , func, begin_fun_decl);
    }
    else
    {
        SNPRINTF(block2str, totalsize - cursize, "%s%s%s %s%s",newline
                   , newline,  pldotnet_GetNetTypeName(rettype, false)
                   , func, begin_fun_decl);
    }

    cursize = strlen(block2str);

    header_nullable = (char *)palloc0(header_size + 1);
    cur_header_size = strlen(header_nullable);

    for (i = 0; i < nargs; i++)
    {
        source_argnm = DirectFunctionCall1(textout,
                DatumGetCString(DatumGetTextP(argname[i])) );

        if (IsNullable(argtype[i]))
        {
            header_nullable_ptr = (char *) (header_nullable + cur_header_size);
            SNPRINTF(header_nullable_ptr, (header_size - cur_header_size) + 1
                       , "%s%s=%s[%d]?(%s)null:%s%s;\n"
                       , pldotnet_GetNullableTypeName(argtype[i])
                       , source_argnm, argsnull_str, i
                       , pldotnet_GetNullableTypeName(argtype[i])
                       , source_argnm, nullable_suffix);
            cur_header_size = strlen(header_nullable);
            argnm = palloc0(strlen(source_argnm) + strlen("_nullable") + 1);
            SNPRINTF(argnm,strlen(source_argnm) + strlen("_nullable") + 1
                            , "%s_nullable", source_argnm);
        }
        else
        {
            argnm = palloc0(strlen(source_argnm) + 1);
            SNPRINTF(argnm,strlen(source_argnm) + 1, "%s", source_argnm);
        }

        argnm_size = strlen(argnm);
        str_ptr = (char *)(block2str + cursize);
        if (i + 1 == nargs)
        {  /* last no comma */
            SNPRINTF(str_ptr, totalsize - cursize, "%s %s"
                , pldotnet_GetNetTypeName(argtype[i], false), argnm);
        }
        else
        {
            SNPRINTF(str_ptr, totalsize - cursize, "%s %s%s"
                , pldotnet_GetNetTypeName(argtype[i], false), argnm, comma);
        }
        cursize = strlen(block2str);
        /* cleaning up for next palloc */
        bzero(argnm,sizeof(argnm));
    }

    str_ptr = (char *)(block2str + cursize);
    SNPRINTF(str_ptr, totalsize - cursize, "%s", end_fun_decl);
    cursize = strlen(block2str);

    if (header_size > 0)
    {
        str_ptr = (char *)(block2str + cursize);
        SNPRINTF(str_ptr, totalsize - cursize, "%s",header_nullable);
        cursize = strlen(block2str);
    }

    str_ptr = (char *)(block2str + cursize);
    SNPRINTF(str_ptr, totalsize - cursize, "%s%s", source_text, end_fun);
    cursize = strlen(block2str);

    if (footer_size > 0)
    {
        str_ptr = (char *)(block2str + cursize);
        SNPRINTF(str_ptr, totalsize - cursize, "%s%s"
            , resu_nullable_value, resu_nullable_flag);
    }

    return block2str;

}

/* Postgres Datum type to C# nullable type name */
static const char *
pldotnet_GetNullableTypeName(Oid id)
{
    switch (id)
    {
        case BOOLOID:
            return "bool?"; /* Nullable<System.Boolean> */
        case INT2OID:
            return "short?";/* Nullable<System.Int16> */
        case INT4OID:
            return "int?";  /* Nullable<System.Int32> */
        case INT8OID:
            return "long?"; /* Nullable<System.Int64> */
    }
    return "";
}

static char *
pldotnet_CreateCStructLibargs(FunctionCallInfo fcinfo, Form_pg_proc procst)
{
    int i;
    int cursize = 0;
    char *libargs_ptr = NULL;
    char *cur_arg = NULL;
    Oid *argtype = procst->proargtypes.values;
    Oid rettype = procst->prorettype;
    Oid type;
    int buff_len;
    char * newargvl;
    /* nullable related */
    bool nullable_arg_flag = false, fcinfo_null_flag;
    bool *argsnull_ptr;
    Datum argdatum;

    dotnet_cstruct_info.typesize_params = 0;
    dotnet_cstruct_info.typesize_nullflags = 0;

    for (i = 0; i < fcinfo->nargs; i++)
    {
        dotnet_cstruct_info.typesize_params += pldotnet_GetTypeSize(argtype[i]);
        if (IsNullable(argtype[i]))
            nullable_arg_flag = true;
    }

    if (nullable_arg_flag)
        dotnet_cstruct_info.typesize_nullflags += sizeof(bool) * fcinfo->nargs;

    dotnet_cstruct_info.typesize_nullflags += sizeof(bool);

    dotnet_cstruct_info.typesize_result = pldotnet_GetTypeSize(rettype);

    libargs_ptr = (char *) palloc0(dotnet_cstruct_info.typesize_nullflags
        + dotnet_cstruct_info.typesize_params
        + dotnet_cstruct_info.typesize_result);

    argsnull_ptr = (bool *) libargs_ptr;
    cur_arg = libargs_ptr + dotnet_cstruct_info.typesize_nullflags;

    for (i = 0; i < fcinfo->nargs; i++)
    {
        type = argtype[i];
#if PG_VERSION_NUM >= 120000
        fcinfo_null_flag=fcinfo->args[i].isnull;
        argdatum = fcinfo->args[i].value;
#else
        fcinfo_null_flag=fcinfo->argnull[i];
        argdatum = fcinfo->arg[i];
#endif
        switch (type)
        {
            case BOOLOID:
                *(bool *)cur_arg = DatumGetBool(argdatum);
                argsnull_ptr[i] = fcinfo_null_flag;
                break;
            case INT4OID:
                *(int *)cur_arg = DatumGetInt32(argdatum);
                argsnull_ptr[i] = fcinfo_null_flag;
                break;
            case INT8OID:
                *(long *)cur_arg = DatumGetInt64(argdatum);
                argsnull_ptr[i] = fcinfo_null_flag;
                break;
            case INT2OID:
                *(short *)cur_arg = DatumGetInt16(argdatum);
                argsnull_ptr[i] = fcinfo_null_flag;
                break;
            case FLOAT4OID:
                *(float *)cur_arg = DatumGetFloat4(argdatum);
                break;
            case FLOAT8OID:
                *(double *)cur_arg = DatumGetFloat8(argdatum);
                break;
            case NUMERICOID:
                /* C String encoding */
                *(unsigned long *)cur_arg =
                    DatumGetCString(DirectFunctionCall1(numeric_out, argdatum));
                break;
            case BPCHAROID:
                /* C String encoding
                 * *(unsigned long *)cur_arg =
                 *    DirectFunctionCall1(bpcharout, DatumGetCString(argdatum));
                 * break;
                 */
            case TEXTOID:
                /* C String encoding
                 * *(unsigned long *)cur_arg =
                 *    DirectFunctionCall1(textout, DatumGetCString(argdatum));
                 * break;
                 */ 
            case VARCHAROID:
                /* C String encoding
                 * *(unsigned long *)cur_arg =
                 *    DirectFunctionCall1(varcharout, DatumGetCString(argdatum));
                 */
               /* UTF8 encoding */
               buff_len = VARSIZE( argdatum ) - VARHDRSZ;
               newargvl = (char *)palloc0(buff_len + 1);
               memcpy(newargvl, VARDATA( argdatum ), buff_len);
               *(unsigned long *)cur_arg = (char *)
                    pg_do_encoding_conversion(newargvl,
                                              buff_len+1,
                                              GetDatabaseEncoding(), PG_UTF8);
                break;

        }
        cursize += pldotnet_GetTypeSize(argtype[i]);
        cur_arg = libargs_ptr + dotnet_cstruct_info.typesize_nullflags + cursize;
    }

    return libargs_ptr;
}

static Datum
pldotnet_GetNetResult(char * libargs, Oid rettype, FunctionCallInfo fcinfo)
{
    Datum retval = 0;
    unsigned long * ret_ptr;
    VarChar * res_varchar; /* For Unicode/UTF8 support */
    int str_len;
    char * str_num;
    char * result_ptr = libargs
                    + dotnet_cstruct_info.typesize_params + dotnet_cstruct_info.typesize_nullflags;
    char * resultnull_ptr = libargs + (dotnet_cstruct_info.typesize_nullflags - sizeof(bool));
    char * encoded_str;

    switch (rettype)
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
            return  Int32GetDatum ( *(int *)(result_ptr) );
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
            return DatumGetNumeric(
                                   DirectFunctionCall3(numeric_in,
                                         CStringGetDatum(str_num),
                                         ObjectIdGetDatum(InvalidOid),
                                         Int32GetDatum(-1)));
        case TEXTOID:
             /* C String encoding
              * retval = DirectFunctionCall1(textin,
              *               CStringGetDatum(
              *                       *(unsigned long *)(libargs + dotnet_cstruct_info.typesize_params)));
              */
        case BPCHAROID:
        /* https://git.brickabode.com/DotNetInPostgreSQL/pldotnet/issues/10#note_19223
         * We should try to get atttymod which is n size in char(n)
         * and use it in bpcharin (I did not find a way to get it)
         * case BPCHAROID:
         *    retval = DirectFunctionCall1(bpcharin,
         *                           CStringGetDatum(
         *                            *(unsigned long *)(libargs + dotnet_cstruct_info.typesize_params)), attypmod);
         */
        case VARCHAROID:
             /* C String encoding
              * retval = DirectFunctionCall1(varcharin,
              *               CStringGetDatum(
              *                       *(unsigned long *)(libargs + dotnet_cstruct_info.typesize_params)));
              */

            /* UTF8 encoding */
            ret_ptr = *(unsigned long *)(result_ptr);
            /* str_len = pg_mbstrlen(ret_ptr); */
            str_len = strlen(ret_ptr);
            encoded_str = pg_do_encoding_conversion( ret_ptr, str_len, PG_UTF8,
                                                    GetDatabaseEncoding() );
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

PG_FUNCTION_INFO_V1(plcsharp_call_handler);
Datum plcsharp_call_handler(PG_FUNCTION_ARGS)
{
    /* return DotNET_callhandler( additional args, fcinfo); */
    bool istrigger;
    char *source_code, *cs_block_call2, *cs_block_call4, *cs_block_call5;
    char *libargs;
    int i, source_code_size;
    HeapTuple proc;
    Form_pg_proc procst;
    Datum retval = 0;
    Oid rettype;

    /* .NET HostFxr declarations */
#ifdef USE_DOTNETBUILD
    char dotnet_type[]  = "DotNetLib.ProcedureClass, DotNetLib";
    char dotnet_type_method[64] = "ProcedureMethod";
    FILE *output_file;
#else
    char dotnet_type[] = "DotNetLib.Lib, DotNetLib";
    char dotnet_type_method[64] = "Compile";
#endif
    char dnldir[] = STR(PLNET_ENGINE_DIR);
    char *root_path;
    int rc;
    load_assembly_and_get_function_pointer_fn load_assembly_and_get_function_pointer;
    component_entry_point_fn csharp_method = nullptr;
    ArgsSource args;

    if (SPI_connect() != SPI_OK_CONNECT)
        elog(ERROR, "[pldotnet]: could not connect to SPI manager");
    istrigger = CALLED_AS_TRIGGER(fcinfo);
    if (istrigger)
    {
        ereport(ERROR,
              (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
               errmsg("[pldotnet]: dotnet trigger not supported")));
    }
    PG_TRY();
    {
        MemoryContext oldcontext = CurrentMemoryContext;
        MemoryContext func_cxt = NULL;
        func_cxt = AllocSetContextCreate(TopMemoryContext,
                                    "PL/NET func_exec_ctx",
                                    ALLOCSET_SMALL_SIZES);
        MemoryContextSwitchTo(func_cxt);
        proc = SearchSysCache(PROCOID
                        , ObjectIdGetDatum(fcinfo->flinfo->fn_oid), 0, 0, 0);
        if (!HeapTupleIsValid(proc))
            elog(ERROR, "[pldotnet]: cache lookup failed for function %u"
                            , (Oid) fcinfo->flinfo->fn_oid);
        procst = (Form_pg_proc) GETSTRUCT(proc);

        /* Build the source code */
        cs_block_call2 = plcsharp_BuildBlock2( procst );
        cs_block_call4 = plcsharp_BuildBlock4( procst );
        cs_block_call5 = plcsharp_BuildBlock5( procst , proc );

        source_code_size = strlen(cs_block_call1) + strlen(cs_block_call2)
            + strlen(cs_block_call3) + strlen(cs_block_call4)
            + strlen(cs_block_call5) + strlen(cs_block_call6) + 1;

        source_code = palloc0(source_code_size);
        SNPRINTF(source_code, source_code_size, "%s%s%s%s%s%s"
            , cs_block_call1, cs_block_call2, cs_block_call3
            , cs_block_call4, cs_block_call5, cs_block_call6);

        rettype = procst->prorettype;

        ReleaseSysCache(proc);
        pfree(cs_block_call2);
        pfree(cs_block_call4);
        pfree(cs_block_call5);

#ifdef USE_DOTNETBUILD
        char *filename;
        const char csharp_srccode_path[] = "/src/csharp/Lib.cs";
        filename = palloc0(strlen(dnldir) + strlen(csharp_srccode_path) + 1);
        SNPRINTF(filename, strlen(dnldir) + strlen(csharp_srccode_path) + 1
                        , "%s%s", dnldir, csharp_srccode_path);
        output_file = fopen(filename, "w");
        if (!output_file)
        {
            fprintf(stderr, "Cannot open file: '%s'\n", filename);
            exit(-1);
        }
        if (fputs(source_code, output_file) == EOF)
        {
            fprintf(stderr, "Cannot write to file: '%s'\n", filename);
            exit(-1);
        }
        fclose(output_file);
        setenv("DOTNET_CLI_HOME", dnldir, 1);
        char *cmd;
        cmd = palloc0(strlen("dotnet build ")
            + strlen(dnldir) + strlen("/src/csharp > null") + 1);
        SNPRINTF(cmd
            , strlen("dotnet build ") + strlen(dnldir) + strlen("/src/csharp > null") + 1
            , "dotnet build %s/src/csharp > null", dnldir);
        int compile_resp = system(cmd);
        assert(compile_resp != -1 && "Failure: Cannot compile C# source code");
#endif

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
        char *config_path;
        const char csharp_json_path[] = "/src/csharp/DotNetLib.runtimeconfig.json";
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
        char *dotnetlib_path;
        const char csharp_dll_path[] = "/src/csharp/DotNetLib.dll";
        dotnetlib_path = palloc0(strlen(root_path) + strlen(csharp_dll_path) + 1);
        SNPRINTF(dotnetlib_path,strlen(root_path) + strlen(csharp_dll_path) + 1
                        , "%s%s", root_path, csharp_dll_path);

        /* Function pointer to managed delegate */
        rc = load_assembly_and_get_function_pointer(
            dotnetlib_path,
            dotnet_type,
            dotnet_type_method,
            nullptr /* delegate_type_name */,
            nullptr,
            (void**)&csharp_method);
        assert(rc == 0 && csharp_method != nullptr && \
            "Failure: load_assembly_and_get_function_pointer()");
        args.SourceCode = source_code;
        args.Result = 1;
#ifndef USE_DOTNETBUILD
        /*
         * STEP 4: Run managed code (Roslyn compiler)
         */
        args.FuncOid = (int) fcinfo->flinfo->fn_oid;
        csharp_method(&args, sizeof(args));
        bzero(dotnet_type_method,sizeof(dotnet_type_method));
        SNPRINTF(dotnet_type_method, strlen("Run") + 1, "%s", "Run");

        /* Function pointer to managed delegate */
        rc = load_assembly_and_get_function_pointer(
            dotnetlib_path,
            dotnet_type,
            dotnet_type_method,
            nullptr /* delegate_type_name */,
            nullptr,
            (void**)&csharp_method);

        assert(rc == 0 && csharp_method != nullptr && \
            "Failure: load_assembly_and_get_function_pointer()");
#endif

        libargs = pldotnet_CreateCStructLibargs(fcinfo, procst);

        csharp_method(libargs,dotnet_cstruct_info.typesize_nullflags +
            dotnet_cstruct_info.typesize_params + dotnet_cstruct_info.typesize_result);
        retval = pldotnet_GetNetResult( libargs, rettype, fcinfo );
        if (libargs != NULL)
            pfree(libargs);
        pfree(source_code);
        MemoryContextSwitchTo(oldcontext);
        if (func_cxt)
            MemoryContextDelete(func_cxt);
    }
    PG_CATCH();
    {
        /* Do the excption handling */
        elog(WARNING, "Exception");
        PG_RE_THROW();
    }
    PG_END_TRY();
    if (SPI_finish() != SPI_OK_FINISH)
        elog(ERROR, "[pldotnet]: could not disconnect from SPI manager");
    return retval;
}

PG_FUNCTION_INFO_V1(plcsharp_validator);
Datum plcsharp_validator(PG_FUNCTION_ARGS)
{
    /* return DotNET_validator( additional args,PG_GETARG_OID(0)); */
    if (SPI_connect() != SPI_OK_CONNECT)
        elog(ERROR, "[pldotnet]: could not connect to SPI manager");
    PG_TRY();
    {
        /* Do some dotnet checking ?? */
    }
    PG_CATCH();
    {
        /* Do the excption handling */
        PG_RE_THROW();
    }
    PG_END_TRY();
    if (SPI_finish() != SPI_OK_FINISH)
        elog(ERROR, "[pldotnet]: could not disconnect from SPI manager");
    return 0; /* VOID */
}


PG_FUNCTION_INFO_V1(plcsharp_inline_handler);
Datum plcsharp_inline_handler(PG_FUNCTION_ARGS)
{
    /* return DotNET_inlinehandler( additional args, CODEBLOCK); */
    if (SPI_connect() != SPI_OK_CONNECT)
        elog(ERROR, "[plldotnet]: could not connect to SPI manager");

    PG_TRY();
    {
        /* Get the current executable's directory
         * This sample assumes the managed assembly to load and its
         * runtime configuration file are next to the host
         */
        int i, source_code_size;
	    char* block_inline3;
        char* source_code;

        /* .NET Hostfxr declarations */
#ifdef USE_DOTNETBUILD
        char dotnet_type[] = "DotNetLib.ProcedureClass, DotNetLib";
        char dotnet_type_method[64] = "ProcedureMethod";
        FILE *output_file;
        int compile_resp;
#else
        char dotnet_type[] = "DotNetLib.Lib, DotNetLib";
        char dotnet_type_method[64] = "Compile";
#endif
        char dnldir[] = STR(PLNET_ENGINE_DIR);
        char* root_path;
        int rc;
        load_assembly_and_get_function_pointer_fn load_assembly_and_get_function_pointer;
        component_entry_point_fn csharp_method = nullptr;
        ArgsSource args;

        block_inline3 = CODEBLOCK;
        source_code_size = strlen(block_inline1) + strlen(block_inline2)
                               + strlen(block_inline3) + strlen(block_inline4) + 1;
        source_code = (char*) palloc0(source_code_size);
	    SNPRINTF(source_code, source_code_size, "%s%s%s%s"
            , block_inline1, block_inline2, block_inline3, block_inline4);
        /*
         * STEP 0: Compile C# source code
         */
#ifdef USE_DOTNETBUILD
        char *filename;
        char csharp_srccode_path[] = "/src/csharp/Lib.cs";
        filename = palloc0(strlen(dnldir) + strlen(csharp_srccode_path) + 1);
        SNPRINTF(filename, strlen(dnldir) + strlen(csharp_srccode_path) + 1
                        , "%s%s", dnldir, csharp_srccode_path);
        output_file = fopen(filename, "w");
        if (!output_file)
        {
            fprintf(stderr, "Cannot open file: '%s'\n", filename);
            exit(-1);
        }
        if (fputs(source_code, output_file) == EOF)
        {
            fprintf(stderr, "Cannot write to file: '%s'\n", filename);
            exit(-1);
        }
        fclose(output_file);
        setenv("DOTNET_CLI_HOME", dnldir, 1);
        char *cmd;
        cmd = palloc0(strlen("dotnet build ")
                        + strlen(dnldir) + strlen("/src/csharp > null") + 1);
        SNPRINTF(cmd
            , strlen("dotnet build ") + strlen(dnldir) + strlen("/src/csharp > null") + 1
            , "dotnet build %s/src/csharp > null", dnldir);
        compile_resp = system(cmd);
        assert(compile_resp != -1 && "Failure: Cannot compile C# source code");
#endif
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
        char *config_path;
        const char csharp_json_path[] = "/src/csharp/DotNetLib.runtimeconfig.json";
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
        char *dotnetlib_path;
        const char csharp_dll_path[] = "/src/csharp/DotNetLib.dll";
        dotnetlib_path = palloc0(strlen(root_path) + strlen(csharp_dll_path) + 1);
        SNPRINTF(dotnetlib_path,strlen(root_path) + strlen(csharp_dll_path) + 1
                        , "%s%s", root_path, csharp_dll_path);
        /* Function pointer to managed delegate */
        rc = load_assembly_and_get_function_pointer(
            dotnetlib_path,
            dotnet_type,
            dotnet_type_method,
            nullptr,/* delegate_type_name */
            nullptr,
            (void**)&csharp_method);
        assert(rc == 0 && csharp_method != nullptr && \
            "Failure: load_assembly_and_get_function_pointer()");
        args.SourceCode = source_code;
        args.Result = 1;
#ifndef USE_DOTNETBUILD
        /*
         * STEP 4: Run managed code (Roslyn compiler)
         */
        csharp_method(&args, sizeof(args));
        bzero(dotnet_type_method,sizeof(dotnet_type_method));
        SNPRINTF(dotnet_type_method, strlen("Run") + 1, "%s", "Run");

        /* Function pointer to managed delegate */
        rc = load_assembly_and_get_function_pointer(
            dotnetlib_path,
            dotnet_type,
            dotnet_type_method,
            nullptr /* delegate_type_name */,
            nullptr,
            (void**)&csharp_method);

        assert(rc == 0 && csharp_method != nullptr && \
            "Failure: load_assembly_and_get_function_pointer()");

#endif
        /*
         * STEP 4: Run managed code (Roslyn compiler)
         */
        csharp_method(&args, sizeof(args));
        pfree(source_code);
    }
    PG_CATCH();
    {
        /* Exception handling */
        PG_RE_THROW();
    }
    PG_END_TRY();

    if (SPI_finish() != SPI_OK_FINISH)
        elog(ERROR, "[pldotnet]: could not disconnect from SPI manager");
    PG_RETURN_VOID();
}
#endif
