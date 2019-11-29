#include "pldotnet.h"
#include <math.h>
#include <mb/pg_wchar.h> //For UTF8 support
#include <utils/numeric.h>

// Statics to hold hostfxr exports
void *h;
static hostfxr_initialize_for_runtime_config_fn init_fptr;
static hostfxr_get_runtime_delegate_fn get_delegate_fptr;
static hostfxr_close_fn close_fptr;

// Forward declarations
static int load_hostfxr();
static load_assembly_and_get_function_pointer_fn get_dotnet_load_assembly(const char_t *assembly);

pldotnet_info dotnet_info;

// TODO: Check how Postgres API do this or shoud we use C lib
#define SNPRINTF(name, size, fmt, ...)                  \
    char name[size];                                    \
    i = snprintf(name, sizeof(name), fmt, __VA_ARGS__); \
    if(i > sizeof(name)){                               \
        fprintf(stderr, "String too long: " #name);     \
        exit(-1);                                       \
    }

PG_MODULE_MAGIC;

// Declare extension variables/structs here
PGDLLEXPORT Datum _PG_init(PG_FUNCTION_ARGS);
PGDLLEXPORT Datum _PG_fini(PG_FUNCTION_ARGS);
PGDLLEXPORT Datum plcsharp_call_handler(PG_FUNCTION_ARGS);
PGDLLEXPORT Datum plcsharp_validator(PG_FUNCTION_ARGS);
#if PG_VERSION_NUM >= 90000
PGDLLEXPORT Datum plcsharp_inline_handler(PG_FUNCTION_ARGS);
#endif

static char * pldotnet_build_block2(Form_pg_proc procst);
static char * pldotnet_build_block4(Form_pg_proc procst);
static char * pldotnet_build_block5(Form_pg_proc procst, HeapTuple proc);
static int get_size_nullable_header(int argNm_size, Oid arg_type, int narg);
static void build_nullable_header(char* src,char* argname, Oid argtype, int narg);
static int get_size_nullable_footer(Oid ret_type);
static void build_nullable_footer(char* src, Oid rettype);
static bool is_nullable(Oid type);
static const char * pldotnet_getNetNullableTypeName(Oid id);
static char * pldotnet_CreateCStrucLibArgs(FunctionCallInfo fcinfo, Form_pg_proc procst);
static Datum pldotnet_getResultFromDotNet(char * libArgs, Oid rettype, FunctionCallInfo fcinfo);
static int get_size_args_null_array(int nargs);
static void build_args_null_array_str(char *dest, int nargs);

#if PG_VERSION_NUM >= 90000
#define CODEBLOCK \
  ((InlineCodeBlock *) DatumGetPointer(PG_GETARG_DATUM(0)))->source_text

const char public_bool[] = "\n[MarshalAs(UnmanagedType.U1)]public ";
const char public_string_utf8[] = "\n[MarshalAs(UnmanagedType.LPUTF8Str)]public ";
const char public_[] = "\npublic ";
/* nullable related constants */
const char resu_nullable_value[] = "libArgs.resu = resu_nullable.GetValueOrDefault();\n";
const char resu_nullable_flag[] = "libArgs.resunull = !resu_nullable.HasValue;\n";
const char argsnull_str[] = "libArgs.argsnull";
const char nullable_suffix[] = "_nullable";
const char resu_flag_str[] = "bool resunull;";
const char arg_flag_str[] = "bool[] argsnull;";

// C# CODE TEMPLATE
char block_call1[] = "                           \n\
using System;                               \n\
using System.Runtime.InteropServices;       \n\
namespace DotNetLib                       \n\
{                                           \n\
    public static class ProcedureClass      \n\
    {                                       \n\
        [StructLayout(LayoutKind.Sequential,Pack=1)]\n\
        public struct LibArgs                \n\
        {";
//block_call2    //public argType1 argName1;
            //public argType2 argName2;
            //...
	        //public returnT resu;//result
char block_call3[] = "                            \n\
        }                                    \n\
        public static int ProcedureMethod(IntPtr arg, int argLength)\n\
        {                                    \n\
            if (argLength != System.Runtime.InteropServices.Marshal.SizeOf(typeof(LibArgs)))\n\
                return 1;                    \n\
            LibArgs libArgs = Marshal.PtrToStructure<LibArgs>(arg);\n";
//block_call4 libArgs.resu = FUNC(libArgs.argName1, libArgs.argName2, ...);
//block_call5    //returnT FUNC(argType1 argName1, argType2 argName2, ...)
	        //{
		          // What is in the SQL function code here
            //}
char  block_call6[] = "                            \n\
            Console.WriteLine($\"\\n\\n\\n resu: {libArgs.resu}\");\n\
            Marshal.StructureToPtr<LibArgs>(libArgs, arg, false);\n\
            return 0;                         \n\
        }                                     \n\
    }                                         \n\
}";


char block_inline1[] = "                           \n\
using System;                               \n\
using System.Runtime.InteropServices;       \n\
namespace DotNetLib                       \n\
{                                           \n\
    public static class ProcedureClass      \n\
    {";
char block_inline2[] = "                    \n\
        public static int ProcedureMethod(IntPtr arg, int argLength)\n\
        {";                                   
//block_inline3   Function body
char  block_inline4[] = "                   \n\
	    return 0; \n\
	}                                   \n\
     }                                      \n\
}";

static int pldotnet_public_decl_size(Oid type)
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

static char * pldotnet_public_decl(Oid type)
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
pldotnet_build_block2(Form_pg_proc procst)
{
    char *block2str, *pStr;
    Oid *argtype = procst->proargtypes.values; // Indicates the args type
    Oid rettype = procst->prorettype; // Indicates the return type
    int nargs = procst->pronargs;
    const char semicon[] = ";";
    char argName[] = " argN";
    char result[] = " resu"; // have to be same size argN
    int i, curSize = 0, totalSize = 0, public_size;
    /* nullable related*/
    bool nullable_arg_flag = false;
    int null_flags_size = 0, return_null_flag_size = 0;
    char *return_null_flag_str;


    if (!pldotnet_type_supported(rettype))
    {
        elog(ERROR, "[pldotnet]: unsupported type on return");
        return 0;
    }

    for (i = 0; i < nargs; i++)
    {
        if (!pldotnet_type_supported(argtype[i]))
        {
            elog(ERROR, "[pldotnet]: unsupported type on arg %d", i);
            return 0;
        }

        if (is_nullable(argtype[i]))
            nullable_arg_flag = true;

        public_size = pldotnet_public_decl_size(argtype[i]);
        totalSize += public_size + strlen(pldotnet_getNetTypeName(argtype[i], true)) +
                       + strlen(argName) + strlen(semicon);
    }
    
    public_size = pldotnet_public_decl_size(rettype);

    return_null_flag_size = strlen(public_bool) + strlen(resu_flag_str);

    if(nullable_arg_flag)
        null_flags_size = get_size_args_null_array(nargs);

    totalSize += public_size + strlen(pldotnet_getNetTypeName(rettype, true)) + 
                    + null_flags_size + return_null_flag_size
                    + strlen(result) + strlen(semicon) + 1;

    block2str = (char *) palloc0(totalSize);

    if(nullable_arg_flag) {
        char *nullable_flags_str = (char *)palloc0(null_flags_size);
        build_args_null_array_str(nullable_flags_str, nargs);
        pStr = (char *)(block2str + curSize);
        sprintf(pStr,"%s",nullable_flags_str);
        curSize += strlen(pStr);
    }

    return_null_flag_str = (char *)palloc0(return_null_flag_size);
    sprintf(return_null_flag_str,"%s%s",public_bool, resu_flag_str);
    pStr = (char *)(block2str + curSize);
    sprintf(pStr,"%s",return_null_flag_str);
    curSize += strlen(pStr);

    for (i = 0; i < nargs; i++)
    {
        sprintf(argName, " arg%d", i); // Review for nargs > 9
        pStr = (char *)(block2str + curSize);
        sprintf(pStr, "%s%s%s%s",
            pldotnet_public_decl(argtype[i]),
            pldotnet_getNetTypeName(argtype[i], true), argName, semicon);
        curSize += strlen(pStr);
    }

    // result
            pStr = (char *)(block2str + curSize);


    sprintf(pStr, "%s%s%s%s",
            pldotnet_public_decl(rettype),
            pldotnet_getNetTypeName(rettype, true), result, semicon);

    /* elog(WARNING, "%s", block2str);*/
    return block2str;
}

static char *
pldotnet_build_block4(Form_pg_proc procst)
{
    char *block2str, *pStr, *resu_var;
    int curSize, i, totalSize;
    const char beginFun[] = "(";
    char * func;
    const char libArgs[] = "libArgs.";
    const char strConvert[] = ".ToString()"; // Converts func return
    const char toDecimal[] = "Convert.ToDecimal(";
    const char comma[] = ",";
    char argName[] = "argN";
    const char endFun[] = ")";
    const char semicolon[] = ";";
    int nargs = procst->pronargs;
    Oid *argtype = procst->proargtypes.values; // Indicates the args type
    Oid rettype = procst->prorettype; // Indicates the return type

    // Function name
    func = NameStr(procst->proname);

    if (is_nullable(rettype))
    {
        const char nullable_result[] = "resu_nullable=";
        resu_var = (char *)palloc0(strlen(pldotnet_getNetNullableTypeName(rettype)) + strlen(nullable_result));
        sprintf(resu_var,"%s%s",pldotnet_getNetNullableTypeName(rettype), nullable_result);
    } else {
        const char result[] = "libArgs.resu=";
        resu_var = (char *)palloc0(strlen(result));
        sprintf(resu_var,"%s",result);
    }

    // TODO:  review for nargs > 9
    if (nargs == 0)
    {
         block2str = (char *)palloc0(strlen(resu_var) + strlen(func) + strlen(beginFun) 
                         + strlen(endFun) + strlen(semicolon));
         if (rettype == NUMERICOID)
            sprintf(block2str, "%s%s%s%s%s%s",resu_var, func, beginFun, endFun, strConvert, semicolon);
         else
            sprintf(block2str, "%s%s%s%s%s",resu_var, func, beginFun, endFun, semicolon);
         return block2str;
    }

    totalSize = strlen(resu_var) + strlen(func) + strlen(beginFun) +
                    (strlen(libArgs) + strlen(argName)) * nargs
                     + strlen(endFun) + strlen(semicolon) + 1;

    for (i = 0; i < nargs; i++) // Get number of Numeric args
    {
        if (argtype[i] == NUMERICOID)
            totalSize += strlen(toDecimal) + strlen(endFun);
    }

    if (rettype == NUMERICOID)
        totalSize += strlen(strConvert);

    if (nargs > 1)
         totalSize += (nargs - 1) * strlen(comma);

    block2str = (char *) palloc0(totalSize);
    sprintf(block2str, "%s%s%s", resu_var, func, beginFun);
    curSize = strlen(resu_var) + strlen(func) + strlen(beginFun);
    for (i = 0; i < nargs; i++)
    {
        sprintf(argName, "arg%d", i); // review nargs > 9
        pStr = (char *)(block2str + curSize);
        if  (i + 1 == nargs)  // last no comma
        {
            if (argtype[i] == NUMERICOID)
                sprintf(pStr, "%s%s%s%s", toDecimal, libArgs, argName, endFun);
            else
                sprintf(pStr, "%s%s", libArgs, argName);
        }
        else {
            if (argtype[i] == NUMERICOID)
                sprintf(pStr, "%s%s%s%s%s", toDecimal, libArgs, argName, endFun, comma);
            else
                sprintf(pStr, "%s%s%s", libArgs, argName, comma);
        }
        curSize = strlen(block2str);
    }

    pStr = (char *)(block2str + curSize);
    if (rettype == NUMERICOID)
        sprintf(pStr, "%s%s%s", endFun, strConvert, semicolon);
    else
        sprintf(pStr, "%s%s", endFun, semicolon);
    /*elog(WARNING, "%s", block2str);*/
    return block2str;

}

static int
get_size_args_null_array(int nargs)
{
    const char public_bool_array[] =
        "\n[MarshalAs(UnmanagedType.ByValArray,ArraySubType=UnmanagedType.U1,SizeConst=)]public ";
    int n_digits_args = 0;

    if(nargs > 0)
        n_digits_args = floor(log10(abs(nargs))) + 1;

    return (strlen(public_bool_array) + n_digits_args + strlen(arg_flag_str));
}

static void
build_args_null_array_str(char *dest, int nargs)
{
    sprintf(dest,"\n[MarshalAs(UnmanagedType.ByValArray,ArraySubType=UnmanagedType.U1,SizeConst=%d)]public %s",
        nargs, arg_flag_str);
}

/*
 * Returns the size of typical C# line converting
 * a struct argument to a nullable C# type argument
 */
static int
get_size_nullable_header(int argNm_size, Oid arg_type, int narg)
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

    if(narg == 0)
        /* Edge case treatment since log10(0) == -HUGE_VAL */
        n_digits_arg = floor(log10(abs(1))) + 1;
    else
        n_digits_arg = floor(log10(abs(narg))) + 1;

    switch (arg_type){
        case INT2OID:
        case INT4OID:
        case INT8OID:
        case BOOLOID:
            /* template: bool? <arg>=argsnull[i]? (bool?)null : <arg>_nullable; */
            total_size = strlen(pldotnet_getNetNullableTypeName(arg_type)) + argNm_size
                + strlen(equal_char) + strlen(argsnull_str) + strlen(square_bracket_char) + n_digits_arg +
                + strlen(square_bracket_char) + strlen(question_mark) + strlen(parenthesis_char)
                + strlen(pldotnet_getNetNullableTypeName(arg_type)) + strlen(parenthesis_char)
                + strlen(null_str) + strlen(colon_char) + argNm_size + strlen(nullable_suffix)
                + strlen(semicolon_char) + strlen(newline_char);
            break;
    }

    return total_size;
}

/*
 * Builds a typical C# line converting
 * a struct to a nullable C# type
 */
static void
build_nullable_header(char* dest,char* argname, Oid argtype, int narg)
{
    /* template:
     *     bool? <arg>=argsnull[i]? (bool?)null : <arg>_nullable;
     */
     switch (argtype){
         case INT2OID:
         case INT4OID:
         case INT8OID:
         case BOOLOID:
             sprintf(dest,"%s%s=%s[%d]?(%s)null:%s%s;\n", pldotnet_getNetNullableTypeName(argtype)
                 , argname, argsnull_str, narg, pldotnet_getNetNullableTypeName(argtype)
                 , argname, nullable_suffix);
     }
}

/*
 * Returns the size of typical C# line converting
 * a nullable C# type return to a struct return
 */
static int
get_size_nullable_footer(Oid ret_type)
{
    int total_size = 0;

    switch (ret_type){
        case INT2OID:
        case INT4OID:
        case INT8OID:
        case BOOLOID:
            total_size = strlen(resu_nullable_value) + strlen(resu_nullable_flag);
            break;
    }

    return total_size;
}

static bool
is_nullable(Oid type)
{
    return (type == INT2OID || type == INT4OID
       || type == INT8OID   ||  type == BOOLOID);
}

/*
 * Build a typical C# line converting
 * a nullable C# type return to a struct return
 */
static void
build_nullable_footer(char* src, Oid rettype)
{
    /* template:
     *     libArgs.resu = resu_nullable.GetValueOrDefault();
     *     libArgs.resunull = !resu_nullable.HasValue;
     */
    switch (rettype){
        case INT2OID:
        case INT4OID:
        case INT8OID:
        case BOOLOID:
            sprintf(src,"%s%s", resu_nullable_value, resu_nullable_flag);
    }
}

static char *
pldotnet_build_block5(Form_pg_proc procst, HeapTuple proc)
{
    char *block2str, *pStr, *argNm, *source_text;
    int argNmSize, i, nnames, curSize, source_size, totalSize;
    bool isnull;
    const char beginFunDec[] = "(";
    char * func;
    const char comma[] = ",";
    const char endFunDec[] = "){\n\n";
    const char endFun[] = "}\n";
    const char newLine[] = "\n";
    int nargs = procst->pronargs;
    Oid rettype = procst->prorettype;
    Datum *argname, argnames, prosrc;
    text * t;
    Oid *argtype = procst->proargtypes.values; // Indicates the args type
    /* nullable related */
    char *header_nullable, *footer_nullable, *header_nullableP;
    int header_size=0, cur_header_size, footer_size=0;

    // Function name
    func = NameStr(procst->proname);

    // Source code
    prosrc = SysCacheGetAttr(PROCOID, proc, Anum_pg_proc_prosrc, &isnull);
    t = DatumGetTextP(prosrc);
    source_text = DirectFunctionCall1(textout, DatumGetCString(t));
    source_size = strlen(source_text);

    argnames = SysCacheGetAttr(PROCOID, proc,
        Anum_pg_proc_proargnames, &isnull);

    if (!isnull)
      deconstruct_array(DatumGetArrayTypeP(argnames), TEXTOID, -1, false,
          'i', &argname, NULL, &nnames);

    // Caculates the total amount in bytes of C# src text for 
    // the function declaration according nr of arguments 
    // their types and the function return type
    if(is_nullable(rettype))
    {
        totalSize = (2 * strlen(newLine)) + strlen(pldotnet_getNetNullableTypeName(rettype))
                    + strlen(" ") + strlen(func) + strlen(beginFunDec);
    } else{
        totalSize = (2 * strlen(newLine)) + strlen(pldotnet_getNetTypeName(rettype, false))
                    + strlen(" ") + strlen(func) + strlen(beginFunDec);
    }

    for (i = 0; i < nargs; i++) 
    {
        argNm = DirectFunctionCall1(textout,
                DatumGetCString(DatumGetTextP(argname[i])) );


        if(is_nullable(argtype[i]))
        {
            header_size += get_size_nullable_header(strlen(argNm),argtype[i],i);
            argNm = strcat(argNm,"_nullable");
        }

        argNmSize = strlen(argNm);
        /*+1 here is the space between type" "argname declaration*/
        totalSize +=  strlen(pldotnet_getNetTypeName(argtype[i], false)) + 1 + argNmSize;
    }
     if (nargs > 1)
         totalSize += (nargs - 1) * strlen(comma); // commas size

    footer_size = get_size_nullable_footer(rettype);

    totalSize += strlen(endFunDec) + header_size + source_size + strlen(endFun) + footer_size + 1;

    block2str = (char *)palloc0(totalSize);

    if(is_nullable(rettype))
    {
        sprintf(block2str, "%s%s%s %s%s",newLine, newLine, pldotnet_getNetNullableTypeName(rettype), func, beginFunDec);
    } else {
        sprintf(block2str, "%s%s%s %s%s",newLine, newLine,  pldotnet_getNetTypeName(rettype, false), func, beginFunDec);
    }

    curSize = strlen(block2str);

    header_nullable = (char *)palloc0(header_size);
    cur_header_size = strlen(header_nullable);

    for (i = 0; i < nargs; i++)
    {
        argNm = DirectFunctionCall1(textout,
                DatumGetCString(DatumGetTextP(argname[i])) );

        if(is_nullable(argtype[i]))
        {
            header_nullableP = (char *) (header_nullable + cur_header_size);
            build_nullable_header(header_nullableP,argNm,argtype[i],i);
            cur_header_size = strlen(header_nullable);
            argNm = strcat(argNm,"_nullable");
        }

        argNmSize = strlen(argNm);
        pStr = (char *)(block2str + curSize);
        if  (i + 1 == nargs)  // last no comma
            sprintf(pStr, "%s %s", pldotnet_getNetTypeName(argtype[i], false), argNm);
        else
            sprintf(pStr, "%s %s%s", pldotnet_getNetTypeName(argtype[i], false), argNm, comma);
        curSize = strlen(block2str);
    }

    pStr = (char *)(block2str + curSize);
    sprintf(pStr, "%s", endFunDec);
    curSize = strlen(block2str);

    if (header_size > 0) {
        pStr = (char *)(block2str + curSize);
        sprintf(pStr, "%s",header_nullable);
        curSize = strlen(block2str);
    }

    pStr = (char *)(block2str + curSize);
    sprintf(pStr, "%s%s", source_text, endFun);
    curSize = strlen(block2str);

    if (footer_size > 0) {
        footer_nullable = (char *)palloc0(footer_size);
        build_nullable_footer(footer_nullable, rettype);
        pStr = (char *)(block2str + curSize);
        sprintf(pStr, "%s", footer_nullable);
    }

    /*elog(WARNING, "%s", block2str);*/
    return block2str;

}

// Postgres Datum type to C# nullable type name
static const char *
pldotnet_getNetNullableTypeName(Oid id)
{
    switch (id){
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
pldotnet_CreateCStrucLibArgs(FunctionCallInfo fcinfo, Form_pg_proc procst)
{
    int i;
    int curSize = 0;
    char *ptrToLibArgs = NULL;
    char *curArg = NULL;
    Oid *argtype = procst->proargtypes.values;
    Oid rettype = procst->prorettype;
    Oid type;
    int lenBuff;
    char * newArgVl;
    /* nullable related*/
    bool nullable_arg_flag = false, fcinfo_null_flag;
    bool *argsnullP;
    Datum argDatum;

    dotnet_info.typeSizeOfParams = 0;
    dotnet_info.typeSizeNullFlags = 0;

    for (i = 0; i < fcinfo->nargs; i++) {
        dotnet_info.typeSizeOfParams += pldotnet_getTypeSize(argtype[i]);
        if(is_nullable(argtype[i]))
            nullable_arg_flag = true;
        /*elog(WARNING, "%d", dotnet_info.typeSizeOfParams );*/
    }

    if(nullable_arg_flag)
        dotnet_info.typeSizeNullFlags += sizeof(bool) * fcinfo->nargs;

    dotnet_info.typeSizeNullFlags += sizeof(bool);

    dotnet_info.typeSizeOfResult = pldotnet_getTypeSize(rettype);

    ptrToLibArgs = (char *) palloc0(dotnet_info.typeSizeNullFlags + dotnet_info.typeSizeOfParams +
                                  dotnet_info.typeSizeOfResult);

    argsnullP = (bool *) ptrToLibArgs;
    curArg = ptrToLibArgs + dotnet_info.typeSizeNullFlags;

    for (i = 0; i < fcinfo->nargs; i++)
    {
        type = argtype[i];
#if PG_VERSION_NUM >= 120000
        fcinfo_null_flag=fcinfo->args[i].isnull;
        argDatum = fcinfo->args[i].value;
#else
        fcinfo_null_flag=fcinfo->argnull[i];
        argDatum = fcinfo->arg[i];
#endif
        switch (type)
        {
            case BOOLOID:
                *(bool *)curArg = DatumGetBool(argDatum);
                argsnullP[i] = fcinfo_null_flag;
                break;
            case INT4OID:
                *(int *)curArg = DatumGetInt32(argDatum);
                argsnullP[i] = fcinfo_null_flag;
                break;
            case INT8OID:
                *(long *)curArg = DatumGetInt64(argDatum);
                argsnullP[i] = fcinfo_null_flag;
                break;
            case INT2OID:
                *(short *)curArg = DatumGetInt16(argDatum);
                argsnullP[i] = fcinfo_null_flag;
                break;
            case FLOAT4OID:
                *(float *)curArg = DatumGetFloat4(argDatum);
                break;
            case FLOAT8OID:
                *(double *)curArg = DatumGetFloat8(argDatum);
                break;
            case NUMERICOID:
                // C String encoding
                *(unsigned long *)curArg =
                    DatumGetCString(DirectFunctionCall1(numeric_out, argDatum));
                break;
            case BPCHAROID:
                // C String encoding
                //*(unsigned long *)curArg =
                //    DirectFunctionCall1(bpcharout, DatumGetCString(argDatum));
                //break;
            case TEXTOID:
                // C String encoding
                //*(unsigned long *)curArg =
                //    DirectFunctionCall1(textout, DatumGetCString(argDatum));
                //break;
            case VARCHAROID:
                // C String encoding
                //*(unsigned long *)curArg =
                //    DirectFunctionCall1(varcharout, DatumGetCString(argDatum));
               // UTF8 encoding
               lenBuff = VARSIZE( argDatum ) - VARHDRSZ;
               newArgVl = (char *)palloc0(lenBuff + 1);
               memcpy(newArgVl, VARDATA( argDatum ), lenBuff);
               *(unsigned long *)curArg = (char *)
                    pg_do_encoding_conversion(newArgVl,
                                              lenBuff+1,
                                              GetDatabaseEncoding(), PG_UTF8);
                break;

        }
        curSize += pldotnet_getTypeSize(argtype[i]);
        curArg = ptrToLibArgs + dotnet_info.typeSizeNullFlags + curSize;
    }

    return ptrToLibArgs;
}

static Datum
pldotnet_getResultFromDotNet(char * libArgs, Oid rettype, FunctionCallInfo fcinfo)
{
    /*elog(WARNING, "nullflags size %d", dotnet_info.typeSizeNullFlags);*/
    /*elog(WARNING, "params size %d", dotnet_info.typeSizeOfParams);*/
    Datum retval = 0;
    unsigned long * retP;
    VarChar * resVarChar; //For Unicode/UTF8 support
    int lenStr;
    char * numStr;
    char * resultP = libArgs
                    + dotnet_info.typeSizeOfParams + dotnet_info.typeSizeNullFlags;
    char * resultNullP = libArgs + (dotnet_info.typeSizeNullFlags - sizeof(bool));
    char * encodedStr;

    switch (rettype){
        case BOOLOID:
            /* Recover flag for null result*/
            fcinfo->isnull = *(bool *) (resultNullP);
            if(fcinfo->isnull)
                return (Datum) 0;
            return  BoolGetDatum  ( *(bool *)(resultP) );
        case INT4OID:
            /* Recover flag for null result*/
            fcinfo->isnull = *(bool *) (resultNullP);
            if(fcinfo->isnull)
                return (Datum) 0;
            return  Int32GetDatum ( *(int *)(resultP) );
        case INT8OID:
            /* Recover flag for null result*/
            fcinfo->isnull = *(bool *) (resultNullP);
            if(fcinfo->isnull)
                return (Datum) 0;
            return  Int64GetDatum ( *(long *)(resultP) );
        case INT2OID:
            /* Recover flag for null result*/
            fcinfo->isnull = *(bool *) (resultNullP);
            if(fcinfo->isnull)
                return (Datum) 0;
            return  Int16GetDatum ( *(short *)(resultP) );
        case FLOAT4OID:
            return  Float4GetDatum ( *(float *)(resultP) );
        case FLOAT8OID:
            return  Float8GetDatum ( *(double *)(resultP) );
        case NUMERICOID:
            numStr = (char *)*(unsigned long *)(resultP);
            return DatumGetNumeric(
                                   DirectFunctionCall3(numeric_in,
                                         CStringGetDatum(numStr),
                                         ObjectIdGetDatum(InvalidOid),
                                         Int32GetDatum(-1)));
        case TEXTOID:
             // C String encoding
             //retval = DirectFunctionCall1(textin,
             //               CStringGetDatum(
             //                       *(unsigned long *)(libArgs + dotnet_info.typeSizeOfParams)));
        case BPCHAROID:
        // https://git.brickabode.com/DotNetInPostgreSQL/pldotnet/issues/10#note_19223
        // We should try to get atttymod which is n size in char(n)
        // and use it in bpcharin (I did not find a way to get it)
        // case BPCHAROID:
        //    retval = DirectFunctionCall1(bpcharin,
        //                           CStringGetDatum(
        //                            *(unsigned long *)(libArgs + dotnet_info.typeSizeOfParams)), attypmod);
        case VARCHAROID:
             // C String encoding
             //retval = DirectFunctionCall1(varcharin,
             //               CStringGetDatum(
             //                       *(unsigned long *)(libArgs + dotnet_info.typeSizeOfParams)));

            // UTF8 encoding
            retP = *(unsigned long *)(resultP);
//          lenStr = pg_mbstrlen(retP);
            lenStr = strlen(retP);
            /*elog(WARNING, "Result size %d", lenStr);*/
            encodedStr = pg_do_encoding_conversion( retP, lenStr, PG_UTF8,
                                                    GetDatabaseEncoding() );
             resVarChar = (VarChar *)SPI_palloc(lenStr + VARHDRSZ);
#if PG_VERSION_NUM < 80300
            VARATT_SIZEP(resVarChar) = lenStr + VARHDRSZ;    /* Total size of structure, not just data */
#else
            SET_VARSIZE(resVarChar, lenStr + VARHDRSZ);      /* Total size of structure, not just data */
#endif
            memcpy(VARDATA(resVarChar), encodedStr , lenStr);
            //pfree(encodedStr);
            PG_RETURN_VARCHAR_P(resVarChar);
    }
    return retval;
}

PG_FUNCTION_INFO_V1(_PG_init);
Datum _PG_init(PG_FUNCTION_ARGS)
{
    // Initialize variable structs here
    // Init dotnet runtime here ?

    elog(LOG, "[plldotnet]: _PG_init");

    PG_RETURN_VOID();
}

PG_FUNCTION_INFO_V1(_PG_fini);
Datum _PG_fini(PG_FUNCTION_ARGS)
{
    // Deinitialize variable/structs here
    // Close dotnet runtime here ?
    dlclose(h);
    PG_RETURN_VOID();
}

PG_FUNCTION_INFO_V1(plcsharp_call_handler);
Datum plcsharp_call_handler(PG_FUNCTION_ARGS)
{
//    return DotNET_callhandler( /* additional args, */ fcinfo);
    bool istrigger;
    char *source_code, *block_call2, *block_call4, *block_call5;
    char *libArgs;
    int i;
    HeapTuple proc;
    Form_pg_proc procst;
    Datum retval = 0;
    Oid rettype;

    // .NET HostFxr declarations
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
    args_source args_source;

    if (SPI_connect() != SPI_OK_CONNECT)
        elog(ERROR, "[pldotnet]: could not connect to SPI manager");
    istrigger = CALLED_AS_TRIGGER(fcinfo);
    if (istrigger) {
        ereport(ERROR,
              (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
               errmsg("[pldotnet]: dotnet trigger not supported")));
    }
    // do dotnet initialization and checks
    // ...
    PG_TRY();
    {
        MemoryContext oldcontext = CurrentMemoryContext;
        MemoryContext func_cxt = NULL;
        func_cxt = AllocSetContextCreate(TopMemoryContext,
                                    "PL/NET func_exec_ctx",
                                    ALLOCSET_SMALL_SIZES);
        MemoryContextSwitchTo(func_cxt);
        proc = SearchSysCache(PROCOID, ObjectIdGetDatum(fcinfo->flinfo->fn_oid), 0, 0, 0);
        if (!HeapTupleIsValid(proc))
            elog(ERROR, "[pldotnet]: cache lookup failed for function %u", (Oid) fcinfo->flinfo->fn_oid);
        procst = (Form_pg_proc) GETSTRUCT(proc);

    // Build the source code
        block_call2 = pldotnet_build_block2( procst );
	//elog(ERROR, "[pldotnet] %s", block_call2);
        block_call4 = pldotnet_build_block4( procst );
	//elog(ERROR, "[pldotnet] %s", block_call4);
        block_call5 = pldotnet_build_block5( procst , proc );
	//elog(ERROR, "[pldotnet] %s", block_call5);
        source_code = palloc0(strlen(block_call1) + strlen(block_call2) + strlen(block_call3)
                             + strlen(block_call4) + strlen(block_call5) + strlen(block_call6) + 1);

        sprintf(source_code, "%s%s%s%s%s%s", block_call1, block_call2, block_call3,
                                             block_call4, block_call5, block_call6);

        /*elog(WARNING, "[pldotnet] %s", source_code);*/

        rettype = procst->prorettype;

        ReleaseSysCache(proc);
        pfree(block_call2);
        pfree(block_call4);
        pfree(block_call5);

#ifdef USE_DOTNETBUILD
        SNPRINTF(filename, 1024, "%s/src/csharp/Lib.cs", dnldir);
        output_file = fopen(filename, "w");
        if (!output_file) {
            fprintf(stderr, "Cannot open file: '%s'\n", filename);
            exit(-1);
        }
        if(fputs(source_code, output_file) == EOF){
            fprintf(stderr, "Cannot write to file: '%s'\n", filename);
            exit(-1);
        }
        fclose(output_file);
        setenv("DOTNET_CLI_HOME", dnldir, 1);
        SNPRINTF(cmd, 1024, "dotnet build %s/src/csharp > null", dnldir);
        int compile_resp = system(cmd);
        assert(compile_resp != -1 && "Failure: Cannot compile C# source code");
#endif

        root_path = strdup(dnldir);
        if(root_path[strlen(root_path) - 1] == DIR_SEPARATOR)
            root_path[strlen(root_path) - 1] = 0;

        //
        // STEP 1: Load HostFxr and get exported hosting functions
        //
        if (!load_hostfxr()) assert(0 && "Failure: load_hostfxr()");

        //
        // STEP 2: Initialize and start the .NET Core runtime
        //
        SNPRINTF(config_path, 1024, "%s/src/csharp/DotNetLib.runtimeconfig.json", root_path);
        fprintf(stderr, "# DEBUG: config_path is '%s'.\n", config_path);

        load_assembly_and_get_function_pointer = get_dotnet_load_assembly(config_path);
        assert(load_assembly_and_get_function_pointer != nullptr && \
            "Failure: get_dotnet_load_assembly()");

        //
        // STEP 3: Load managed assembly and get function pointer to a managed method
        //
        SNPRINTF(dotnetlib_path, 1024, "%s/src/csharp/DotNetLib.dll", root_path);
        fprintf(stderr, "# DEBUG: dotnetlib_path is '%s'.\n", dotnetlib_path);
        // Function pointer to managed delegate
        rc = load_assembly_and_get_function_pointer(
            dotnetlib_path,
            dotnet_type,
            dotnet_type_method,
            nullptr /*delegate_type_name*/,
            nullptr,
            (void**)&csharp_method);
        assert(rc == 0 && csharp_method != nullptr && \
            "Failure: load_assembly_and_get_function_pointer()");
        args_source.SourceCode = source_code;
        args_source.Result = 1;
#ifndef USE_DOTNETBUILD
        //
        // STEP 4: Run managed code (Roslyn compiler)
        //
        args_source.FuncOid = (int) fcinfo->flinfo->fn_oid;
        csharp_method(&args_source, sizeof(args_source));
        bzero(dotnet_type_method,sizeof(dotnet_type_method));
        strcpy(dotnet_type_method,"Run");

        // Function pointer to managed delegate
        rc = load_assembly_and_get_function_pointer(
            dotnetlib_path,
            dotnet_type,
            dotnet_type_method,
            nullptr /*delegate_type_name*/,
            nullptr,
            (void**)&csharp_method);

        assert(rc == 0 && csharp_method != nullptr && \
            "Failure: load_assembly_and_get_function_pointer()");
#endif
        /*elog(WARNING, "start create c struc");*/

        libArgs = pldotnet_CreateCStrucLibArgs(fcinfo, procst);
        /*elog(WARNING, "libargs size: %d", dotnet_info.typeSizeNullFlags +
            dotnet_info.typeSizeOfParams + dotnet_info.typeSizeOfResult);*/
        csharp_method(libArgs,dotnet_info.typeSizeNullFlags +
            dotnet_info.typeSizeOfParams + dotnet_info.typeSizeOfResult);
        retval = pldotnet_getResultFromDotNet( libArgs, rettype, fcinfo );
        if (libArgs != NULL)
            pfree(libArgs);
        pfree(source_code);
        MemoryContextSwitchTo(oldcontext);
        if (func_cxt)
            MemoryContextDelete(func_cxt);
    }
    PG_CATCH();
    {
        // Do the excption handling
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
//    return DotNET_validator(/* additional args, */ PG_GETARG_OID(0));
    if (SPI_connect() != SPI_OK_CONNECT)
        elog(ERROR, "[pldotnet]: could not connect to SPI manager");
    PG_TRY();
    {
        // Do some dotnet checking ??
    }
    PG_CATCH();
    {
        // Do the excption handling
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
    //  return DotNET_inlinehandler( /* additional args, */ CODEBLOCK);
    if (SPI_connect() != SPI_OK_CONNECT)
        elog(ERROR, "[plldotnet]: could not connect to SPI manager");

    PG_TRY();
    {
        // Run dotnet anonymous here  CODEBLOCK has the inlined source code

        // Get the current executable's directory
        // This sample assumes the managed assembly to load and its
        // runtime configuration file are next to the host
        int i;
	    char* block_inline3;
        char* source_code;

        // .NET Hostfxr declarations
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
        args_source args;

        block_inline3 = CODEBLOCK;
        source_code = (char*) palloc0(strlen(block_inline1) + strlen(block_inline2)
			                   + strlen(block_inline3) + strlen(block_inline4) + 1);
	    sprintf(source_code, "%s%s%s%s", block_inline1, block_inline2, block_inline3, block_inline4);

	    //elog(WARNING,"AFTERSPF: %s\n\n\n",source_code);
	    //
        // STEP 0: Compile C# source code
        //
#ifdef USE_DOTNETBUILD
        SNPRINTF(filename, 1024, "%s/src/csharp/Lib.cs", dnldir);
        output_file = fopen(filename, "w");
        if (!output_file) {
            fprintf(stderr, "Cannot open file: '%s'\n", filename);
            exit(-1);
        }
        if(fputs(source_code, output_file) == EOF){
            fprintf(stderr, "Cannot write to file: '%s'\n", filename);
            exit(-1);
        }
        fclose(output_file);
        setenv("DOTNET_CLI_HOME", dnldir, 1);
        SNPRINTF(cmd, 1024, "dotnet build %s/src/csharp > null", dnldir);
        compile_resp = system(cmd);
        assert(compile_resp != -1 && "Failure: Cannot compile C# source code");
#endif
        root_path = strdup(dnldir);
        if(root_path[strlen(root_path) - 1] == DIR_SEPARATOR)
            root_path[strlen(root_path) - 1] = 0;

        //
        // STEP 1: Load HostFxr and get exported hosting functions
        //
        if (!load_hostfxr()) assert(0 && "Failure: load_hostfxr()");

        //
        // STEP 2: Initialize and start the .NET Core runtime
        //
        SNPRINTF(config_path, 1024, "%s/src/csharp/DotNetLib.runtimeconfig.json", root_path);
        fprintf(stderr, "# DEBUG: config_path is '%s'.\n", config_path);

        load_assembly_and_get_function_pointer = get_dotnet_load_assembly(config_path);
        assert(load_assembly_and_get_function_pointer != nullptr && \
            "Failure: get_dotnet_load_assembly()");

        //
        // STEP 3: Load managed assembly and get function pointer to a managed method
        //
        SNPRINTF(dotnetlib_path, 1024, "%s/src/csharp/DotNetLib.dll", root_path);
        fprintf(stderr, "# DEBUG: dotnetlib_path is '%s'.\n", dotnetlib_path);
        // Function pointer to managed delegate
        rc = load_assembly_and_get_function_pointer(
            dotnetlib_path,
            dotnet_type,
            dotnet_type_method,
            nullptr,//delegate_type_name
            nullptr,
            (void**)&csharp_method);
        assert(rc == 0 && csharp_method != nullptr && \
            "Failure: load_assembly_and_get_function_pointer()");
        args.SourceCode = source_code;
        args.Result = 1;
#ifndef USE_DOTNETBUILD
        //
        // STEP 4: Run managed code (Roslyn compiler)
        //
        csharp_method(&args, sizeof(args));

	    bzero(dotnet_type_method,sizeof(dotnet_type_method));
	    strcpy(dotnet_type_method,"Run");
        // Function pointer to managed delegate
        rc = load_assembly_and_get_function_pointer(
            dotnetlib_path,
            dotnet_type,
            dotnet_type_method,
            nullptr /*delegate_type_name*/,
            nullptr,
            (void**)&csharp_method);

        assert(rc == 0 && csharp_method != nullptr && \
            "Failure: load_assembly_and_get_function_pointer()");

#endif
        //
        // STEP 4: Run managed code (Roslyn compiler)
        //
        csharp_method(&args, sizeof(args));
        pfree(source_code);
    }
    PG_CATCH();
    {
        // Exception handling
        PG_RE_THROW();
    }
    PG_END_TRY();

    if (SPI_finish() != SPI_OK_FINISH)
        elog(ERROR, "[pldotnet]: could not disconnect from SPI manager");
    PG_RETURN_VOID();
}

/********************************************************************************************
 * Function used to load and activate .NET Core
 ********************************************************************************************/
// Forward declarations
static void *pldotnet_load_library(const char_t *);
static void *pldotnet_get_export(void *, const char *);

static void *
pldotnet_load_library(const char_t *path)
{
    fprintf(stderr, "# DEBUG: doing dlopen(%s).\n", path);
    h = dlopen(path, RTLD_LAZY | RTLD_LOCAL);
    assert(h != nullptr);
    return h;
}

static void *
pldotnet_get_export(void *h, const char *name)
{
    void *f = dlsym(h, name);
    if(f == nullptr){
        fprintf(stderr, "Can't dlsym(%s); exiting.\n", name);
        exit(-1);
    }
    return f;
}

// Using the nethost library, discover the location of hostfxr and get exports
static int
load_hostfxr()
{
    // Pre-allocate a large buffer for the path to hostfxr
    char_t buffer[MAX_PATH];
    size_t buffer_size = sizeof(buffer) / sizeof(char_t);
    int rc = get_hostfxr_path(buffer, &buffer_size, nullptr);
    void *lib;
    if (rc != 0)
        return 0;

    // Load hostfxr and get desired exports
    lib = pldotnet_load_library(buffer);
    init_fptr = (hostfxr_initialize_for_runtime_config_fn)pldotnet_get_export( \
        lib, "hostfxr_initialize_for_runtime_config");
    get_delegate_fptr = (hostfxr_get_runtime_delegate_fn)pldotnet_get_export( \
        lib, "hostfxr_get_runtime_delegate");
    close_fptr = (hostfxr_close_fn)pldotnet_get_export(lib, "hostfxr_close");

    return (init_fptr && get_delegate_fptr && close_fptr);
}

// Load and initialize .NET Core and get desired function pointer for scenario
static load_assembly_and_get_function_pointer_fn
get_dotnet_load_assembly(const char_t *config_path)
{
    // Load .NET Core
    void *load_assembly_and_get_function_pointer = nullptr;
    hostfxr_handle cxt = nullptr;
    int rc = init_fptr(config_path, nullptr, &cxt);
    if (rc > 1 || rc < 0 || cxt == nullptr)
    {
        fprintf(stderr, "Init failed: %x\n", rc);
        close_fptr(cxt);
        return nullptr;
    }

    // Get the load assembly function pointer
    rc = get_delegate_fptr(
        cxt,
        hdt_load_assembly_and_get_function_pointer,
        &load_assembly_and_get_function_pointer);
    if (rc != 0 || load_assembly_and_get_function_pointer == nullptr)
        fprintf(stderr, "Get delegate failed: %x\n", rc);
    close_fptr(cxt);
    return (load_assembly_and_get_function_pointer_fn)load_assembly_and_get_function_pointer;
}


#endif
