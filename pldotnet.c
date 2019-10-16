#include "pldotnet.h"

//// DOTNET-HOST-SAMPLE STUFF ///////////////////////////////////////
// Provided by the AppHost NuGet package and installed as an SDK pack
#include <assert.h>

#include <nethost.h>
// Header files copied from https://github.com/dotnet/core-setup
#include <coreclr_delegates.h>
#include <hostfxr.h>
#include <dlfcn.h>
#include <limits.h>

#define STR(s) s
#define CH(c) c
#define DIR_SEPARATOR '/'
#define MAX_PATH PATH_MAX
// Null pointer constant definition
#define nullptr ((void*)0)

// Statics to hold hostfxr exports
void *h;
static hostfxr_initialize_for_runtime_config_fn init_fptr;
static hostfxr_get_runtime_delegate_fn get_delegate_fptr;
static hostfxr_close_fn close_fptr;

// Forward declarations
static int load_hostfxr();
static load_assembly_and_get_function_pointer_fn get_dotnet_load_assembly(const char_t *assembly);

// TODO: Check how Postgres API do this or shoud we use C lib
#define SNPRINTF(name, size, fmt, ...)                  \
    char name[size];                                    \
    i = snprintf(name, sizeof(name), fmt, __VA_ARGS__); \
    if(i > sizeof(name)){                               \
        fprintf(stderr, "String too long: " #name);     \
        exit(-1);                                       \
    }
//// DOTNET-HOST-SAMPLE STUFF ///////////////////////////////////////

PG_MODULE_MAGIC;

// Declare extension variables/structs here

PGDLLEXPORT Datum _PG_init(PG_FUNCTION_ARGS);
PGDLLEXPORT Datum _PG_fini(PG_FUNCTION_ARGS);
PGDLLEXPORT Datum pldotnet_call_handler(PG_FUNCTION_ARGS);
PGDLLEXPORT Datum pldotnet_validator(PG_FUNCTION_ARGS);
#if PG_VERSION_NUM >= 90000
PGDLLEXPORT Datum pldotnet_inline_handler(PG_FUNCTION_ARGS);
#endif

static char * pldotnet_build_block2(Form_pg_proc procst);
static char * pldotnet_build_block4(Form_pg_proc procst);
static char * pldotnet_build_block5(Form_pg_proc procst, HeapTuple proc);
static int * pldotnet_CreateCStrucLibArgs(FunctionCallInfo fcinfo, Form_pg_proc procst);
static int pldotnet_getArgTypeSize(void * p);
static Datum pldotnet_getResultFromDotNet(int * libArgs);

typedef struct pldotnet_info
{
    char * libArgs;
    int    typeSizeOfParams;
    int    typeSizeOfResult;
}pldotnet_info;
pldotnet_info dotnet_info;

#if PG_VERSION_NUM >= 90000
#define CODEBLOCK \
  ((InlineCodeBlock *) DatumGetPointer(PG_GETARG_DATUM(0)))->source_text

// C# CODE TEMPLATE
char block1[] = "                           \n\
using System;                               \n\
using System.Runtime.InteropServices;       \n\
namespace DotNetLib                         \n\
{                                           \n\
    public static class Lib                 \n\
    {                                       \n\
        [StructLayout(LayoutKind.Sequential)]\n\
        public struct LibArgs                \n\
        {";
//block2    //public argType1 argName1;
            //public argType2 argName2;
            //...
	        //public returnT resu;//result
char block3[] = "                            \n\
        }                                    \n\
        public static int Main(IntPtr arg, int argLength)\n\
        {                                    \n\
            if (argLength < System.Runtime.InteropServices.Marshal.SizeOf(typeof(LibArgs)))\n\
                return 1;                    \n\
            LibArgs libArgs = Marshal.PtrToStructure<LibArgs>(arg);\n\
            libArgs.resu = ";
//block4                     //FUNC(libArgs.argName1, libArgs.argName2, ...);
//block5    //returnT FUNC(argType1 argName1, argType2 argName2, ...)
	        //{
		          // What is in the SQL function code here
            //}
char  block6[] = "                            \n\
            Console.WriteLine($\"resu: {libArgs.resu}\");\n\
	        Marshal.StructureToPtr<LibArgs>(libArgs, arg, false);\n\
            return 0;                         \n\
        }                                     \n\
    }                                         \n\
}";

// TODO: We should calculate first the size of things then we will
//       do only one malloc instead of keep reallocing

static char *
pldotnet_build_block2(Form_pg_proc procst)
{
    char *block2str, *pStr;
    int i;
    Oid *argtype = procst->proargtypes.values; // Indicates the args type
    Oid rettype = procst->prorettype; // Indicates the return type
    int nargs = procst->pronargs;
    const char public_[] = "public ";
    const char semicon[] = ";";
    const char int_[] = "int ";
    char argName[] = "argN";
    char result[] = "resu"; // have to be same size argN
    int curSize = 0;
    int declSize = strlen(public_) + strlen(int_) + strlen(argName) + strlen(semicon);

    int totalSize = (declSize * nargs) + declSize/*result*/;

    if (rettype != INT4OID) // TODO: check for all supported types
    {
        elog(ERROR, "[pldotnet]: unsupported type on return");
        return 0;
    }

    block2str = (char *) malloc(totalSize);
    for (i = 0; i < nargs; i++)
    {
        if (argtype[i] != INT4OID) // TODO: check for all supporte types
        {
            // Unsupported type
            elog(ERROR, "[pldotnet]: unsupported type on arg %d", i);
            if (block2str != NULL)
                free(block2str);
            return 0;
        }
        sprintf(argName, "arg%d", i); // Review for nargs > 9
        pStr = (char *)(block2str + curSize);
        sprintf(pStr, "%s%s%s%s", public_, int_, argName, semicon);
        curSize += declSize;
    }

    // result
    pStr = (char *)(block2str + curSize);
    sprintf(pStr, "%s%s%s%s", public_, int_, result, semicon);
    elog(WARNING, "%s", block2str);
    return block2str;
}

static char *
pldotnet_build_block4(Form_pg_proc procst)
{
    char *block2str, *pStr;
    int curSize, i, totalSize;
    const char func[] = "FUNC(";
    const char libArgs[] = "libArgs.";
    const char comma[] = ",";
    char argName[] = "argN";
    const char endFun[] = ");";
    int declParamSize = strlen(libArgs) + strlen(argName);
    int declParamSizeComma = declParamSize + strlen(comma);
    int nargs = procst->pronargs;

    // TODO:  review for nargs > 9
    if (nargs == 0)
    {
         block2str = (char *)malloc(strlen(func) + strlen(endFun));
         sprintf(block2str, "%s%s", func, endFun);
         return block2str;
    }

    totalSize = strlen(func) + (strlen(libArgs) + strlen(argName)) * nargs 
                     + strlen(endFun);

    if (nargs > 1)
         totalSize += (nargs - 1) * strlen(comma);

    block2str = (char *) malloc(totalSize);
    sprintf(block2str, "%s", func);
    curSize = strlen(func);
    for (i = 0; i < nargs; i++)
    {
        sprintf(argName, "arg%d", i); // review nargs > 9
        if  (i + 1 == nargs)  // last no comma
        {
            pStr = (char *)(block2str + curSize);
            sprintf(pStr, "%s%s", libArgs, argName);
            curSize += declParamSize;
        }
        else {
            pStr = (char *)(block2str + curSize);
            sprintf(pStr, "%s%s%s", libArgs, argName, comma);
            curSize += declParamSizeComma;
        }
    }

    pStr = (char *)(block2str + curSize);
    sprintf(pStr, "%s", endFun);
    elog(WARNING, "%s", block2str);
    return block2str;

}

static char *
pldotnet_build_block5(Form_pg_proc procst, HeapTuple proc)
{
    char *block2str, *pStr, *argNm, *source_text;
    int argNmSize, i, nnames, curSize, source_size, totalSize;
    bool isnull;
    const char intret_[] = "\nint ";
    const char func[] = "FUNC(";
    const char int_[] = "int ";
    const char comma[] = ",";
    const char endFunDec[] = "){";
    const char endFun[] = "}\n";
    int nargs = procst->pronargs;
    //Oid rettype = procst->prorettype; // Indicates the return type
    int declParamSize = strlen(intret_);
    Datum *argname, argnames, prosrc;
    text * t;

    // Source code
    prosrc = SysCacheGetAttr(PROCOID, proc, Anum_pg_proc_prosrc, &isnull);
    t = DatumGetTextP(prosrc);
    source_text = VARDATA(t);
    source_size = VARSIZE(t) - VARHDRSZ;

    argnames = SysCacheGetAttr(PROCOID, proc,
        Anum_pg_proc_proargnames, &isnull);

    if (!isnull)
      deconstruct_array(DatumGetArrayTypeP(argnames), TEXTOID, -1, false,
          'i', &argname, NULL, &nnames);

    totalSize = strlen(intret_) + strlen(func) + strlen(int_) * nargs
                + strlen(endFunDec) + source_size + strlen(endFun);

    for (i = 0; i < nargs; i++)
    {
        argNmSize = VARSIZE(DatumGetTextP(argname[i])) - VARHDRSZ;
        totalSize += argNmSize;
    }

    block2str = (char *)malloc(totalSize);
    sprintf(block2str, "%s%s", intret_, func);
    curSize = declParamSize + strlen(func);
    for (i = 0; i < nargs; i++)
    {
        argNm = VARDATA(DatumGetTextP(argname[i]));
        argNmSize = VARSIZE(DatumGetTextP(argname[i])) - VARHDRSZ;
        if  (i + 1 == nargs)  // last no comma
        {
            declParamSize = strlen(int_) + argNmSize;
            pStr = (char *)(block2str + curSize);
            sprintf(pStr, "%s%s", int_, argNm);
        }
        else
        {
            declParamSize = strlen(int_) + argNmSize + strlen(comma);
            pStr = (char *)(block2str + curSize);
            sprintf(pStr, "%s%s%s", int_, argNm, comma);
        }
        curSize += declParamSize;
    }

    pStr = (char *)(block2str + curSize);
    sprintf(pStr, "%s%s%s", endFunDec, source_text, endFun);
    elog(WARNING, "%s", block2str);

    return block2str;

}

static int
pldotnet_getArgTypeSize(void * p)
{
    return 4; // only for integers
}

static int *
pldotnet_CreateCStrucLibArgs(FunctionCallInfo fcinfo, Form_pg_proc procst)
{
    //Oid rettype = procst->prorettype; // Indicates the return type
    int nargs = procst->pronargs;
    int *curArg;
    int sizeOf_, i;
    int totalSize = 0;
    int *ptrToLibArgs = NULL;

    if (nargs == 0)
    {
        // Only the return
        sizeOf_ = pldotnet_getArgTypeSize(NULL);
        //ptrToLibArgs = malloc(sizeOf_);
        ptrToLibArgs = (int *) malloc(sizeof(int));
        dotnet_info.typeSizeOfParams = 0;
        //dotnet_info.typeSizeOfResult = sizeOf_;
        dotnet_info.typeSizeOfResult = sizeof(int);
        return ptrToLibArgs;
    }

    for (i = 0; i < fcinfo->nargs; i++)
    {
        sizeOf_ = pldotnet_getArgTypeSize(NULL);
        if (i == 0)
        {
            ptrToLibArgs = malloc(sizeOf_);
            // Apply here DatumGet<type> (postgres.h)
            *(int *)ptrToLibArgs = DatumGetInt32(fcinfo->arg[i]);
            totalSize = sizeOf_;
        }
        else
        {
            ptrToLibArgs = realloc(ptrToLibArgs, totalSize + sizeOf_);
            curArg = ptrToLibArgs + totalSize / sizeof(int);
            *(int *)curArg = DatumGetInt32(fcinfo->arg[i]);
            totalSize += sizeOf_;
        }
    }
    dotnet_info.typeSizeOfParams = totalSize;
    // Return
    sizeOf_ = pldotnet_getArgTypeSize(NULL);
    dotnet_info.typeSizeOfResult = sizeOf_;
    ptrToLibArgs = realloc(ptrToLibArgs, totalSize + sizeOf_);
    return ptrToLibArgs;
}

static Datum
pldotnet_getResultFromDotNet(int * libArgs)
{
    elog(WARNING, "---> %d", *(libArgs+dotnet_info.typeSizeOfParams / sizeof(int)));
    elog(WARNING, "params size %d", dotnet_info.typeSizeOfParams);
    return  Int32GetDatum ( *(libArgs + dotnet_info.typeSizeOfParams / sizeof(int)) );
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

PG_FUNCTION_INFO_V1(pldotnet_call_handler);
Datum pldotnet_call_handler(PG_FUNCTION_ARGS)
{
//    return DotNET_callhandler( /* additional args, */ fcinfo);
    bool istrigger;
    char *source_code, *block2, *block4, *block5;
    int *libArgs;
    int source_size, i;
    HeapTuple proc;
    Form_pg_proc procst;
    Datum retval = 0;

    char default_dnldir[] = "/var/lib/DotNetLib/";

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
        proc = SearchSysCache(PROCOID, ObjectIdGetDatum(fcinfo->flinfo->fn_oid), 0, 0, 0);
        if (!HeapTupleIsValid(proc))
            elog(ERROR, "[pldotnet]: cache lookup failed for function %u", (Oid) fcinfo->flinfo->fn_oid);
        procst = (Form_pg_proc) GETSTRUCT(proc);

        // Build the source code
        block2 = pldotnet_build_block2( procst );
	//elog(ERROR, "[pldotnet] %s", block2);
        block4 = pldotnet_build_block4( procst );
	//elog(ERROR, "[pldotnet] %s", block4);
        block5 = pldotnet_build_block5( procst , proc );
	//elog(ERROR, "[pldotnet] %s", block5);
        source_code = malloc(strlen(block1) + strlen(block2) + strlen(block3)
                          + strlen(block4) + strlen(block5) + strlen(block6) + 1);

        sprintf(source_code, "%s%s%s%s%s%s", block1, block2, block3,
                                             block4, block5, block6);

	elog(WARNING, "[pldotnet] %s", source_code);

        ReleaseSysCache(proc);
        free(block2);
        free(block4);
        free(block5);

        char *dnldir = getenv("DNLDIR");
        if (dnldir == nullptr) dnldir = &default_dnldir[0];
        SNPRINTF(filename, 1024, "%s/src/Lib.cs", dnldir);

        FILE *output_file = fopen(filename, "w");
        if (!output_file) {
            fprintf(stderr, "Cannot open file: '%s'\n", filename);
            exit(-1);
        }

        if(fputs(source_code, output_file) == EOF){
            fprintf(stderr, "Cannot write to file: '%s'\n", filename);
            exit(-1);
        }
        fclose(output_file);

        setenv("DOTNET_CLI_HOME", default_dnldir, 1);
        SNPRINTF(cmd, 1024, "dotnet build %s/src > nul", dnldir);
        int compile_resp = system(cmd);
        assert(compile_resp != -1 && "Failure: Cannot compile C# source code");

        char* root_path = strdup(dnldir);
        char *last_separator = rindex(root_path, DIR_SEPARATOR);
        if(last_separator != NULL) *(last_separator+1) = 0;

        //
        // STEP 1: Load HostFxr and get exported hosting functions
        //
        if (!load_hostfxr()) assert(0 && "Failure: load_hostfxr()");

        //
        // STEP 2: Initialize and start the .NET Core runtime
        //
        SNPRINTF(config_path, 1024, "%s/src/DotNetLib.runtimeconfig.json", root_path);
        fprintf(stderr, "# DEBUG: config_path is '%s'.\n", config_path);

        load_assembly_and_get_function_pointer_fn load_assembly_and_get_function_pointer = \
            get_dotnet_load_assembly(config_path);
        assert(load_assembly_and_get_function_pointer != nullptr && \
            "Failure: get_dotnet_load_assembly()");

        //
        // STEP 3: Load managed assembly and get function pointer to a managed method
        //
        SNPRINTF(dotnetlib_path, 1024, "%s/src/DotNetLib.dll", root_path);
        char dotnet_type[]        = "DotNetLib.Lib, DotNetLib";
        char dotnet_type_method[] = "Main";
        fprintf(stderr, "# DEBUG: dotnetlib_path is '%s'.\n", dotnetlib_path);

        // <SnippetLoadAndGet>
        // Function pointer to managed delegate
        component_entry_point_fn csharp_main = nullptr;
        int rc = load_assembly_and_get_function_pointer(
            dotnetlib_path,
            dotnet_type,
            dotnet_type_method,
            nullptr /*delegate_type_name*/,
            nullptr,
            (void**)&csharp_main);
        // </SnippetLoadAndGet>

        assert(rc == 0 && csharp_main != nullptr && \
            "Failure: load_assembly_and_get_function_pointer()");

        //
        // STEP 4: Run managed code
        //
        elog(WARNING, "start create c struc");
        libArgs = pldotnet_CreateCStrucLibArgs(fcinfo, procst);
        elog(WARNING, "libargs size: %d", dotnet_info.typeSizeOfParams + dotnet_info.typeSizeOfResult);
        csharp_main(libArgs, dotnet_info.typeSizeOfParams + dotnet_info.typeSizeOfResult);
        retval = pldotnet_getResultFromDotNet( libArgs );
        if (libArgs != NULL);
            free(libArgs);
        free(source_code);
    }
    PG_CATCH();
    {
        // Do the excption handling
        PG_RE_THROW();
    }
    PG_END_TRY();
    if (SPI_finish() != SPI_OK_FINISH)
        elog(ERROR, "[pldotnet]: could not disconnect from SPI manager");
    return retval;
}

PG_FUNCTION_INFO_V1(pldotnet_validator);
Datum pldotnet_validator(PG_FUNCTION_ARGS)
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


PG_FUNCTION_INFO_V1(pldotnet_inline_handler);
Datum pldotnet_inline_handler(PG_FUNCTION_ARGS)
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

        // char* resolved = realpath(argv[0], host_path);
        // assert(resolved != nullptr);

        //// DOTNET-HOST-SAMPLE STUFF ///////////////////////////////////////
        const char* source_code = CODEBLOCK;
        //
        // STEP 0: Compile C# source code
        //
        char default_dnldir[] = "/var/lib/DotNetLib/";
        char *dnldir = getenv("DNLDIR");
	if (dnldir == nullptr) dnldir = &default_dnldir[0];
        SNPRINTF(filename, 1024, "%s/src/Lib.cs", dnldir);

        FILE *output_file = fopen(filename, "w+");
        if (!output_file) {
            fprintf(stderr, "Cannot open file: '%s'\n", filename);
            exit(-1);
        }
        if(fputs(source_code, output_file) == EOF){
            fprintf(stderr, "Cannot write to file: '%s'\n", filename);
            exit(-1);
        }
        fclose(output_file);

        setenv("DOTNET_CLI_HOME", default_dnldir, 1);
        SNPRINTF(cmd, 1024, "dotnet build %s/src > nul", dnldir);
        int compile_resp = system(cmd);
        assert(compile_resp != -1 && "Failure: Cannot compile C# source code");

        char* root_path = strdup(dnldir);
        char *last_separator = rindex(root_path, DIR_SEPARATOR);
        if(last_separator != NULL) *(last_separator+1) = 0;

        //
        // STEP 1: Load HostFxr and get exported hosting functions
        //
        if (!load_hostfxr()) assert(0 && "Failure: load_hostfxr()");

        //
        // STEP 2: Initialize and start the .NET Core runtime
        //
        SNPRINTF(config_path, 1024, "%s/src/DotNetLib.runtimeconfig.json", root_path);
        fprintf(stderr, "# DEBUG: config_path is '%s'.\n", config_path);

        load_assembly_and_get_function_pointer_fn load_assembly_and_get_function_pointer = \
            get_dotnet_load_assembly(config_path);
        assert(load_assembly_and_get_function_pointer != nullptr && \
            "Failure: get_dotnet_load_assembly()");

        //
        // STEP 3: Load managed assembly and get function pointer to a managed method
        //
        SNPRINTF(dotnetlib_path, 1024, "%s/src/DotNetLib.dll", root_path);
        char dotnet_type[]        = "DotNetLib.Lib, DotNetLib";
        char dotnet_type_method[] = "Main";
        fprintf(stderr, "# DEBUG: dotnetlib_path is '%s'.\n", dotnetlib_path);

        // <SnippetLoadAndGet>
        // Function pointer to managed delegate
        component_entry_point_fn csharp_main = nullptr;
        int rc = load_assembly_and_get_function_pointer(
            dotnetlib_path,
            dotnet_type,
            dotnet_type_method,
            nullptr,//delegate_type_name
            nullptr,
            (void**)&csharp_main);
        // </SnippetLoadAndGet>

        assert(rc == 0 && csharp_main != nullptr && \
            "Failure: load_assembly_and_get_function_pointer()");

        //
        // STEP 4: Run managed code
        //
        struct lib_args
        {
            int number1;
            int number2;
            int Result;
        };

        for (i = 0; i < 3; ++i)
        {
            // <SnippetCallManaged>
            struct lib_args args =  { .number1 = i, .number2 = i };
            csharp_main(&args, sizeof(args));
            printf("Sum from C#: %d\n",args.Result);
            // </SnippetCallManaged>
        }

        //// DOTNET-HOST-SAMPLE STUFF ///////////////////////////////////////
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

//// DOTNET-HOST-SAMPLE STUFF ///////////////////////////////////////
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

// <SnippetLoadHostFxr>
// Using the nethost library, discover the location of hostfxr and get exports
static int
load_hostfxr()
{
    // Pre-allocate a large buffer for the path to hostfxr
    char_t buffer[MAX_PATH];
    size_t buffer_size = sizeof(buffer) / sizeof(char_t);
    int rc = get_hostfxr_path(buffer, &buffer_size, nullptr);
    if (rc != 0)
        return 0;

    // Load hostfxr and get desired exports
    void *lib = pldotnet_load_library(buffer);
    init_fptr = (hostfxr_initialize_for_runtime_config_fn)pldotnet_get_export( \
        lib, "hostfxr_initialize_for_runtime_config");
    get_delegate_fptr = (hostfxr_get_runtime_delegate_fn)pldotnet_get_export( \
        lib, "hostfxr_get_runtime_delegate");
    close_fptr = (hostfxr_close_fn)pldotnet_get_export(lib, "hostfxr_close");

    return (init_fptr && get_delegate_fptr && close_fptr);
}
// </SnippetLoadHostFxr>

// <SnippetInitialize>
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

// </SnippetInitialize>
//// DOTNET-HOST-SAMPLE STUFF ///////////////////////////////////////
#endif

