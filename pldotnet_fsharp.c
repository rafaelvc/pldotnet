#include "pldotnet_common.h"
#include <mb/pg_wchar.h> //For UTF8 support
#include <utils/numeric.h>
#include "pldotnet_fsharp.h"

// Statics to hold hostfxr exports
void *h;
static hostfxr_initialize_for_runtime_config_fn init_fptr;
static hostfxr_get_runtime_delegate_fn get_delegate_fptr;
static hostfxr_close_fn close_fptr;

// Forward declarations
static int load_hostfxr();
static load_assembly_and_get_function_pointer_fn get_dotnet_load_assembly(const char_t *assembly);

PGDLLEXPORT Datum plfsharp_call_handler(PG_FUNCTION_ARGS);
PGDLLEXPORT Datum plfsharp_validator(PG_FUNCTION_ARGS);
#if PG_VERSION_NUM >= 90000
PGDLLEXPORT Datum plfsharp_inline_handler(PG_FUNCTION_ARGS);
#endif

// TODO: Check how Postgres API do this or shoud we use C lib
#define SNPRINTF(name, size, fmt, ...)                  \
    char name[size];                                    \
    i = snprintf(name, sizeof(name), fmt, __VA_ARGS__); \
    if(i > sizeof(name)){                               \
        fprintf(stderr, "String too long: " #name);     \
        exit(-1);                                       \
    }

pldotnet_info dotnet_info;

char * plfsharp_build_block2(Form_pg_proc procst);
char * plfsharp_build_block4(Form_pg_proc procst, HeapTuple proc);
char * plfsharp_build_block6(Form_pg_proc procst);
char * plfsharp_CreateCStrucLibArgs(FunctionCallInfo fcinfo, Form_pg_proc procst);
Datum plfsharp_getResultFromDotNet(char * libArgs, Oid rettype, FunctionCallInfo fcinfo);

char fs_block_call1[] = "\n\
namespace DotNetLib      \n\
    open System.Runtime.InteropServices\n\
    [<Struct>]           \n\
    [<StructLayout (LayoutKind.Sequential)>]\n\
    type LibArgs =\n";
/****** fs_block_call2 ******
 *      val mutable arg1:int
 *      val mutable arg2:int
 *      ...
 *      val mutable resu:int
 */
char fs_block_call3[] = "\n\
    type Lib =\n";
/********* fs_block_call4 ******
 *         static member <function_name> =
 *             <function_body>
 */
char fs_block_call5[] = "\n\
        static member ProcedureMethod (arg:System.IntPtr) (argLength:int) = \n\
           let mutable libArgs = Marshal.PtrToStructure<LibArgs> arg\n";

char fs_block_call7[] = "\n\
           Marshal.StructureToPtr(libArgs, arg, false)\n\
           0";

char *
plfsharp_build_block2(Form_pg_proc procst)
{
    char *block2str, *pStr;
    Oid *argtype = procst->proargtypes.values; // Indicates the args type
    Oid rettype = procst->prorettype; // Indicates the return type
    int nargs = procst->pronargs;
    const char val[] = "        val mutable";
    const char colon[] = ":";
    char argName[] = " argN";
    char result[] = " resu"; // have to be same size argN
    int i, curSize = 0, totalSize = 0;

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

        totalSize += strlen(val) + strlen(argName) + strlen(colon)
                        + strlen(pldotnet_getNetTypeName(argtype[i], true));
    }

    totalSize += strlen(val) + strlen(result) + strlen(colon)
                    + strlen(pldotnet_getNetTypeName(rettype, true)) + 1;

    block2str = (char *) palloc0(totalSize);

    for (i = 0; i < nargs; i++)
    {
        sprintf(argName, " arg%d", i); // Review for nargs > 9
        pStr = (char *)(block2str + curSize);
        sprintf(pStr, "%s%s%s%s\n",
            val, argName, colon,
            pldotnet_getNetTypeName(argtype[i], true) );
        curSize += strlen(pStr);
    }

    pStr = (char *)(block2str + curSize);

    sprintf(pStr, "%s%s%s%s",
            val, result, colon,
            pldotnet_getNetTypeName(rettype, true));

    return block2str;
}

char *
plfsharp_build_block4(Form_pg_proc procst, HeapTuple proc)
{
    char *block2str, *pStr, *argNm, *source_text;
    int argNmSize, i, nnames, curSize, source_size, totalSize;
    bool isnull;
    char *func;
    const char member[] = "static member ";
    const char equal[] = " =\n";
    const char func_sign_indent[] = "        ";
    const char func_body_indent[] = "            ";
    char *user_line;
    const char endFunDec[] = " =\n";
    const char endFun[] = "\n";
    int nargs = procst->pronargs;
    Oid rettype = procst->prorettype;
    Datum *argname, argnames, prosrc;
    text * t;
    Oid *argtype = procst->proargtypes.values; // Indicates the args type

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

    totalSize = strlen(func_sign_indent) + strlen(member) + strlen(func) + strlen(" ");

    for (i = 0; i < nargs; i++) 
    {
        argNm = DirectFunctionCall1(textout,
                DatumGetCString(DatumGetTextP(argname[i])) );

        argNmSize = strlen(argNm);
        /*+1 here is the space between type" "argname declaration*/
        totalSize +=  1 + argNmSize;
    }

    /* tokenizes source_code into its lines for indentation insertion*/
    user_line = strtok(source_text,"\n");
    while(user_line != NULL){
        user_line = strtok(NULL,"\n");
    }

    totalSize +=  strlen(endFunDec) + source_size + strlen(endFun);

    block2str = (char *)palloc0(totalSize);

    sprintf(block2str, "%s%s%s ",func_sign_indent, member, func);

    curSize = strlen(block2str);

    for (i = 0; i < nargs; i++)
    {
        argNm = DirectFunctionCall1(textout,
                DatumGetCString(DatumGetTextP(argname[i])) );

        argNmSize = strlen(argNm);
        pStr = (char *)(block2str + curSize);

        sprintf(pStr, " %s",argNm);
        curSize = strlen(block2str);
    }


    pStr = (char *)(block2str + curSize);
    sprintf(pStr, "%s", endFunDec);
    curSize = strlen(block2str);

    user_line = strtok(source_text,"\n");
    while(user_line != NULL){
        pStr = (char *)(block2str + curSize);
        sprintf(pStr,"%s%s",func_body_indent,user_line);
        user_line = strtok(NULL,"\n");
        curSize = strlen(block2str);
    }

    pStr = (char *)(block2str + curSize);
    sprintf(pStr, "%s", endFun);

    return block2str;
}

char *
plfsharp_build_block6(Form_pg_proc procst)
{
    char *block2str, *pStr, *resu_var;
    int curSize, i, totalSize;
    char * func;
    const char libArgs[] = " libArgs.";
    char argName[] = "argN";
    const char endFun[] = "\n";
    const char func_line_indent[] = "           ";
    const char result[] = "libArgs.resu <- Lib.";
    int nargs = procst->pronargs;
    Oid *argtype = procst->proargtypes.values; // Indicates the args type
    Oid rettype = procst->prorettype; // Indicates the return type

    // Function name
    func = NameStr(procst->proname);

    // TODO:  review for nargs > 9
    if (nargs == 0)
    {
         block2str = (char *)palloc0(strlen(func_line_indent) + strlen(result) + strlen(func) + strlen(endFun));
         sprintf(block2str, "%s%s%s%s",func_line_indent,resu_var, func, endFun);
    }

    totalSize = strlen(func_line_indent) +strlen(result) + strlen(func) 
                    + (strlen(libArgs) + strlen(argName)) * nargs
                    + strlen(endFun);

    block2str = (char *) palloc0(totalSize);
    sprintf(block2str, "%s%s%s",func_line_indent, result, func);
    curSize = strlen(block2str);

    for (i = 0; i < nargs; i++)
    {
        sprintf(argName, "arg%d", i); // review nargs > 9
        pStr = (char *)(block2str + curSize);
        sprintf(pStr, "%s%s", libArgs, argName);
        curSize = strlen(block2str);
    }

    pStr = (char *)(block2str + curSize);
    sprintf(pStr, "%s", endFun);

    return block2str;
}

char *
plfsharp_CreateCStrucLibArgs(FunctionCallInfo fcinfo, Form_pg_proc procst)
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
    bool fcinfo_null_flag;
    Datum argDatum;

    dotnet_info.typeSizeOfParams = 0;

    for (i = 0; i < fcinfo->nargs; i++) {
        dotnet_info.typeSizeOfParams += pldotnet_getTypeSize(argtype[i]);
    }

    dotnet_info.typeSizeOfResult = pldotnet_getTypeSize(rettype);

    ptrToLibArgs = (char *) palloc0(dotnet_info.typeSizeOfParams +
                                  dotnet_info.typeSizeOfResult);

    curArg = ptrToLibArgs;

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
                break;
            case INT4OID:
                *(int *)curArg = DatumGetInt32(argDatum);
                break;
            case INT8OID:
                *(long *)curArg = DatumGetInt64(argDatum);
                break;
            case INT2OID:
                *(short *)curArg = DatumGetInt16(argDatum);
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
        curArg = ptrToLibArgs + curSize;
    }

    return ptrToLibArgs;
}

Datum
plfsharp_getResultFromDotNet(char * libArgs, Oid rettype, FunctionCallInfo fcinfo)
{
    Datum retval = 0;
    unsigned long * retP;
    VarChar * resVarChar; //For Unicode/UTF8 support
    int lenStr;
    char * numStr;
    char * resultP = libArgs
                    + dotnet_info.typeSizeOfParams;
    char * encodedStr;

    switch (rettype){
        case BOOLOID:
            /* Recover flag for null result*/
            return  BoolGetDatum  ( *(bool *)(resultP) );
        case INT4OID:
            /* Recover flag for null result*/
            return  Int32GetDatum ( *(int *)(resultP) );
        case INT8OID:
            /* Recover flag for null result*/
            return  Int64GetDatum ( *(long *)(resultP) );
        case INT2OID:
            /* Recover flag for null result*/
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

//////////// FSharp handlers
PG_FUNCTION_INFO_V1(plfsharp_call_handler);
Datum plfsharp_call_handler(PG_FUNCTION_ARGS)
{
    bool istrigger;
    char *source_code,
         *fs_block_call2,
         *fs_block_call4,
         *fs_block_call6;
    char *libArgs;
    int i;
    HeapTuple proc;
    Form_pg_proc procst;
    Datum retval = 0;
    Oid rettype;

    // .NET HostFxr declarations
    char dotnet_type[]  = "DotNetLib.Lib, DotNetLib";
    char dotnet_type_method[64] = "ProcedureMethod";
    FILE *output_file;
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
    PG_TRY();
    {
        // Do F# handler here
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
        fs_block_call2 = plfsharp_build_block2( procst );
//	elog(ERROR, "[pldotnet] %s", block_call2);
       fs_block_call4 = plfsharp_build_block4( procst , proc );
	//elog(ERROR, "[pldotnet] %s", block_call4);
        fs_block_call6 = plfsharp_build_block6( procst);
	//elog(ERROR, "[pldotnet] %s", block_call5);
        source_code = palloc0(strlen(fs_block_call1) + strlen(fs_block_call2) + strlen(fs_block_call3)
                             + strlen(fs_block_call4) + strlen(fs_block_call5) + strlen(fs_block_call6)
                             + strlen(fs_block_call7) + 1);

        sprintf(source_code, "%s%s%s%s%s%s%s",fs_block_call1, fs_block_call2, fs_block_call3,
                                            fs_block_call4, fs_block_call5, fs_block_call6, fs_block_call7);

        rettype = procst->prorettype;

        ReleaseSysCache(proc);
        pfree(fs_block_call2);
        pfree(fs_block_call4);
        pfree(fs_block_call6);

        SNPRINTF(filename, 1024, "%s/src/fsharp/Lib.fs", dnldir);
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
        SNPRINTF(cmd, 1024, "dotnet build %s/src/fsharp > null", dnldir);
        int compile_resp = system(cmd);
        assert(compile_resp != -1 && "Failure: Cannot compile C# source code");

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
        SNPRINTF(config_path, 1024, "%s/src/fsharp/DotNetLib.runtimeconfig.json", root_path);
        fprintf(stderr, "# DEBUG: config_path is '%s'.\n", config_path);

        load_assembly_and_get_function_pointer = get_dotnet_load_assembly(config_path);
        assert(load_assembly_and_get_function_pointer != nullptr && \
            "Failure: get_dotnet_load_assembly()");

        //
        // STEP 3: Load managed assembly and get function pointer to a managed method
        //
        SNPRINTF(dotnetlib_path, 1024, "%s/src/fsharp/DotNetLib.dll", root_path);
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
        /*elog(WARNING, "start create c struc");*/

        libArgs = plfsharp_CreateCStrucLibArgs(fcinfo, procst);
        /*elog(WARNING, "libargs size: %d", dotnet_info.typeSizeNullFlags +
            dotnet_info.typeSizeOfParams + dotnet_info.typeSizeOfResult);*/
        csharp_method(libArgs,dotnet_info.typeSizeNullFlags +
            dotnet_info.typeSizeOfParams + dotnet_info.typeSizeOfResult);
        retval = plfsharp_getResultFromDotNet( libArgs, rettype, fcinfo );
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

PG_FUNCTION_INFO_V1(plfsharp_validator);
Datum plfsharp_validator(PG_FUNCTION_ARGS)
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

PG_FUNCTION_INFO_V1(plfsharp_inline_handler);
Datum plfsharp_inline_handler(PG_FUNCTION_ARGS)
{
    //  return DotNET_inlinehandler( /* additional args, */ CODEBLOCK);
    if (SPI_connect() != SPI_OK_CONNECT)
        elog(ERROR, "[plldotnet]: could not connect to SPI manager");

    PG_TRY();
    {
        // Do F# handler here
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

