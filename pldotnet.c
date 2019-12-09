#include <postgres.h>
#include "pldotnet.h"
#include "pldotnet_hostfxr.h"
#include <funcapi.h>
#include <dlfcn.h>

PG_MODULE_MAGIC;

/* Declare extension variables/structs here */
PGDLLEXPORT Datum _PG_init(PG_FUNCTION_ARGS);
PGDLLEXPORT Datum _PG_fini(PG_FUNCTION_ARGS);
#if PG_VERSION_NUM >= 90000
#define CODEBLOCK \
  ((InlineCodeBlock *) DatumGetPointer(PG_GETARG_DATUM(0)))->source_text
PG_FUNCTION_INFO_V1(_PG_init);
Datum _PG_init(PG_FUNCTION_ARGS)
{
    /* Initialize variable structs here
     * Init dotnet runtime here ?
     */

    elog(LOG, "[plldotnet]: _PG_init");

    PG_RETURN_VOID();
}

PG_FUNCTION_INFO_V1(_PG_fini);
Datum _PG_fini(PG_FUNCTION_ARGS)
{
    /* Deinitialize variable/structs here
     * Close dotnet runtime here ?
     */

    dlclose(nethost_lib);
    PG_RETURN_VOID();
}
#endif
