#include "pldotnet_fsharp.h"
#include <mb/pg_wchar.h> //For UTF8 support
#include <utils/numeric.h>

PGDLLEXPORT Datum plfsharp_call_handler(PG_FUNCTION_ARGS);
PGDLLEXPORT Datum plfsharp_validator(PG_FUNCTION_ARGS);
#if PG_VERSION_NUM >= 90000
PGDLLEXPORT Datum plfsharp_inline_handler(PG_FUNCTION_ARGS);
#endif

static pldotnet_info dotnet_info;

static char * plfsharp_build_block2(Form_pg_proc procst);
static char * plfsharp_build_block4(Form_pg_proc procst, HeapTuple proc);
static char * plfsharp_build_block6(Form_pg_proc procst);
static char * plfsharp_CreateCStrucLibArgs(FunctionCallInfo fcinfo, Form_pg_proc procst);
static Datum plfsharp_getResultFromDotNet(char * libArgs, Oid rettype, FunctionCallInfo fcinfo);

static char fs_block_call1[] = "\n\
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
static char fs_block_call3[] = "\n\
    type Lib =\n";
/********* fs_block_call4 ******
 *         static member <function_name> =
 *             <function_body>
 */
static char fs_block_call5[] = "\n\
        static member ProcedureMethod (arg:System.IntPtr) (argLength:int) = \n\
           let mutable libArgs = Marshal.PtrToStructure<LibArgs> arg\n";

static char fs_block_call7[] = "\n\
           Marshal.StructureToPtr(libArgs, arg, false)\n\
           0";

static bool
plfsharp_type_supported(Oid type)
{
    return type == INT4OID;
}

static char *
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

    if (!plfsharp_type_supported(rettype))
    {
        elog(ERROR, "[pldotnet]: unsupported type on return");
        return 0;
    }

    for (i = 0; i < nargs; i++)
    {
        if (!plfsharp_type_supported(argtype[i]))
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
        SNPRINTF(argName, strlen(argName)+1, " arg%d", i); // Review for nargs > 9
        pStr = (char *)(block2str + curSize);
        SNPRINTF(pStr,totalSize - curSize, "%s%s%s%s\n",
                   val, argName, colon,
                   pldotnet_getNetTypeName(argtype[i], true) );
        curSize += strlen(pStr);
    }

    pStr = (char *)(block2str + curSize);

    SNPRINTF(pStr, totalSize - curSize, "%s%s%s%s",
                val, result, colon,
                pldotnet_getNetTypeName(rettype, true));

    return block2str;
}

static char *
plfsharp_build_block4(Form_pg_proc procst, HeapTuple proc)
{
    char *block2str, *pStr, *argNm, *source_text;
    int argNmSize, i, nnames, curSize=0, totalSize;
    bool isnull;
    char *func;
    const char member[] = "static member ";
    const char func_signature_indent[] = "        ";
    const char func_body_indent[] = "            ";
    char *user_line;
    const char endFunDec[] = " =\n";
    const char endFun[] = "\n";
    int nargs = procst->pronargs;
    Datum *argname, argnames, prosrc;
    text * t;

    // Function name
    func = NameStr(procst->proname);

    // Source code
    prosrc = SysCacheGetAttr(PROCOID, proc, Anum_pg_proc_prosrc, &isnull);
    t = DatumGetTextP(prosrc);
    source_text = DirectFunctionCall1(textout, DatumGetCString(t));

    argnames = SysCacheGetAttr(PROCOID, proc,
        Anum_pg_proc_proargnames, &isnull);

    if (!isnull)
      deconstruct_array(DatumGetArrayTypeP(argnames), TEXTOID, -1, false,
          'i', &argname, NULL, &nnames);

    // Caculates the total amount in bytes of F# src text for 
    // the function declaration according nr of arguments 
    // and function body necessary indentation 

    totalSize = strlen(func_signature_indent) + strlen(member) + strlen(func) + strlen(" ");

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
        totalSize += strlen(func_body_indent) + strlen(user_line);
        user_line = strtok(NULL,"\n");
    }

    totalSize += strlen(endFunDec) + strlen(endFun) + 1;

    block2str = (char *)palloc0(totalSize);

    SNPRINTF(block2str, totalSize - curSize, "%s%s%s ",func_signature_indent, member, func);

    curSize = strlen(block2str);

    for (i = 0; i < nargs; i++)
    {
        argNm = DirectFunctionCall1(textout,
                DatumGetCString(DatumGetTextP(argname[i])) );

        argNmSize = strlen(argNm);
        pStr = (char *)(block2str + curSize);

        SNPRINTF(pStr, totalSize - curSize, " %s",argNm);
        curSize = strlen(block2str);
    }


    pStr = (char *)(block2str + curSize);
    SNPRINTF(pStr, totalSize - curSize, "%s", endFunDec);
    curSize = strlen(block2str);

    user_line = strtok(source_text,"\n");
    while(user_line != NULL){
        pStr = (char *)(block2str + curSize);
        SNPRINTF(pStr, totalSize - curSize, "%s%s",func_body_indent,user_line);
        user_line = strtok(NULL,"\n");
        curSize = strlen(block2str);
    }

    pStr = (char *)(block2str + curSize);
    SNPRINTF(pStr, totalSize - curSize, "%s", endFun);

    return block2str;
}

static char *
plfsharp_build_block6(Form_pg_proc procst)
{
    char *block2str, *pStr, *resu_var;
    int curSize = 0, i, totalSize;
    char * func;
    const char libArgs[] = " libArgs.";
    char argName[] = "argN";
    const char endFun[] = "\n";
    const char func_line_indent[] = "           ";
    const char result[] = "libArgs.resu <- Lib.";
    int nargs = procst->pronargs;

    // Function name
    func = NameStr(procst->proname);

    // TODO:  review for nargs > 9
    if (nargs == 0)
    {
         int block_size = strlen(func_line_indent) + strlen(result) + strlen(func) + strlen(endFun) + 1;
         block2str = (char *)palloc0(block_size);
         SNPRINTF(block2str, block_size, "%s%s%s%s",func_line_indent,result, func, endFun);
         return block2str;
    }

    totalSize = strlen(func_line_indent) +strlen(result) + strlen(func) 
                    + (strlen(libArgs) + strlen(argName)) * nargs
                    + strlen(endFun) + 1;

    block2str = (char *) palloc0(totalSize);
    SNPRINTF(block2str, totalSize - curSize, "%s%s%s",func_line_indent, result, func);
    curSize = strlen(block2str);

    for (i = 0; i < nargs; i++)
    {
        SNPRINTF(argName, strlen(argName) + 1, "arg%d", i); // review nargs > 9
        pStr = (char *)(block2str + curSize);
        SNPRINTF(pStr, totalSize - curSize, "%s%s", libArgs, argName);
        curSize = strlen(block2str);
    }

    pStr = (char *)(block2str + curSize);
    SNPRINTF(pStr, totalSize - curSize, "%s", endFun);

    return block2str;
}

static char *
plfsharp_CreateCStrucLibArgs(FunctionCallInfo fcinfo, Form_pg_proc procst)
{
    int i;
    int curSize = 0;
    char *ptrToLibArgs = NULL;
    char *curArg = NULL;
    Oid *argtype = procst->proargtypes.values;
    Oid rettype = procst->prorettype;
    Oid type;
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
        argDatum = fcinfo->args[i].value;
#else
        argDatum = fcinfo->arg[i];
#endif
        switch (type)
        {
            case INT4OID:
                *(int *)curArg = DatumGetInt32(argDatum);
                break;
        }
        curSize += pldotnet_getTypeSize(argtype[i]);
        curArg = ptrToLibArgs + curSize;
    }

    return ptrToLibArgs;
}

static Datum
plfsharp_getResultFromDotNet(char * libArgs, Oid rettype, FunctionCallInfo fcinfo)
{
    Datum retval = 0;
    char * resultP = libArgs
                    + dotnet_info.typeSizeOfParams;

    switch (rettype){
        case INT4OID:
            /* Recover flag for null result*/
            return  Int32GetDatum ( *(int *)(resultP) );
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
    int i, source_code_size;
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
    component_entry_point_fn fsharp_method = nullptr;

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
        fs_block_call4 = plfsharp_build_block4( procst , proc );
        fs_block_call6 = plfsharp_build_block6( procst);

        source_code_size = strlen(fs_block_call1) + strlen(fs_block_call2) + strlen(fs_block_call3)
                               + strlen(fs_block_call4) + strlen(fs_block_call5) + strlen(fs_block_call6)
                               + strlen(fs_block_call7) + 1;

        source_code = palloc0(source_code_size);
        SNPRINTF(source_code, source_code_size, "%s%s%s%s%s%s%s",fs_block_call1, fs_block_call2, fs_block_call3,
                                            fs_block_call4, fs_block_call5, fs_block_call6, fs_block_call7);

        rettype = procst->prorettype;

        ReleaseSysCache(proc);
        pfree(fs_block_call2);
        pfree(fs_block_call4);
        pfree(fs_block_call6);

        char *filename;
        filename = palloc0(strlen(dnldir) + strlen("/src/fsharp/Lib.fs") + 1);
        SNPRINTF(filename, strlen(dnldir) + strlen("/src/fsharp/Lib.fs") + 1
                        , "%s/src/fsharp/Lib.fs", dnldir);
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
        char *cmd;
        cmd = palloc0(strlen("dotnet build ") + strlen(dnldir) + strlen("/src/csharp > null") + 1);
        SNPRINTF(cmd, strlen("dotnet build ") + strlen(dnldir) + strlen("/src/fsharp > null") + 1
                    , "dotnet build %s/src/fsharp > null", dnldir);
        int compile_resp = system(cmd);
        assert(compile_resp != -1 && "Failure: Cannot compile C# source code");

        root_path = strdup(dnldir);
        if(root_path[strlen(root_path) - 1] == DIR_SEPARATOR)
            root_path[strlen(root_path) - 1] = 0;

        //
        // STEP 1: Load HostFxr and get exported hosting functions
        //
        if (!pldotnet_load_hostfxr()) assert(0 && "Failure: pldotnet_load_hostfxr()");

        //
        // STEP 2: Initialize and start the .NET Core runtime
        //
        char *config_path;
        config_path = palloc0(strlen(root_path) + strlen("/src/fsharp/DotNetLib.runtimeconfig.json") + 1);
        SNPRINTF(config_path, strlen(root_path) + strlen("/src/fsharp/DotNetLib.runtimeconfig.json") + 1
                        , "%s/src/fsharp/DotNetLib.runtimeconfig.json", root_path);

        load_assembly_and_get_function_pointer = get_dotnet_load_assembly(config_path);
        assert(load_assembly_and_get_function_pointer != nullptr && \
            "Failure: get_dotnet_load_assembly()");

        //
        // STEP 3: Load managed assembly and get function pointer to a managed method
        //
        char *dotnetlib_path;
        dotnetlib_path = palloc0(strlen(root_path) + strlen("/src/fsharp/DotNetLib.dll") + 1);
        SNPRINTF(dotnetlib_path,strlen(root_path) + strlen("/src/fsharp/DotNetLib.dll") + 1
                        , "%s/src/fsharp/DotNetLib.dll", root_path);
        // Function pointer to managed delegate
        rc = load_assembly_and_get_function_pointer(
            dotnetlib_path,
            dotnet_type,
            dotnet_type_method,
            nullptr /*delegate_type_name*/,
            nullptr,
            (void**)&fsharp_method);
        assert(rc == 0 && fsharp_method != nullptr && \
            "Failure: load_assembly_and_get_function_pointer()");

        libArgs = plfsharp_CreateCStrucLibArgs(fcinfo, procst);
        fsharp_method(libArgs,dotnet_info.typeSizeNullFlags +
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
