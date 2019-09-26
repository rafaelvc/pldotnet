#include "pldotnet.h"

PG_MODULE_MAGIC;

// Declare extension variables/structs here

PGDLLEXPORT Datum _PG_init(PG_FUNCTION_ARGS);
PGDLLEXPORT Datum _PG_fini(PG_FUNCTION_ARGS);
PGDLLEXPORT Datum pldotnet_call_handler(PG_FUNCTION_ARGS);
PGDLLEXPORT Datum pldotnet_validator(PG_FUNCTION_ARGS);
#if PG_VERSION_NUM >= 90000
PGDLLEXPORT Datum pldotnet_inline_handler(PG_FUNCTION_ARGS);
#endif

PG_FUNCTION_INFO_V1(_PG_init);
Datum _PG_init(PG_FUNCTION_ARGS) 
{
    // Initialize variable structs here 
    // Init dotnet runtime here ?
    PG_RETURN_VOID();
}

PG_FUNCTION_INFO_V1(_PG_fini);
Datum _PG_fini(PG_FUNCTION_ARGS) 
{
    // Deinitialize variable/structs here
    // Close dotnet runtime here ?
    PG_RETURN_VOID();
}

PG_FUNCTION_INFO_V1(pldotnet_call_handler);
Datum pldotnet_call_handler(PG_FUNCTION_ARGS) 
{
//    return DotNET_callhandler( /* additional args, */ fcinfo);
    Datum retval = 0;
    bool istrigger;
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
        // dot dotnet magic
        // how to get args ? fcinfo
        //retval = //(dotnet result of computation)
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

#if PG_VERSION_NUM >= 90000
#define CODEBLOCK \
  ((InlineCodeBlock *) DatumGetPointer(PG_GETARG_DATUM(0)))->source_text

PG_FUNCTION_INFO_V1(pldotnet_inline_handler);
Datum pldotnet_inline_handler(PG_FUNCTION_ARGS) 
{
//  return DotNET_inlinehandler( /* additional args, */ CODEBLOCK);
    if (SPI_connect() != SPI_OK_CONNECT)
        elog(ERROR, "[plldotnet]: could not connect to SPI manager");
    PG_TRY();
    {
        // Run dotnet anonymous here  CODEBLOCK has the inlined source code
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
#endif






