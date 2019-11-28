#include "pldotnetcommon.h"
#include "pldotnet_utils.h"
#include <assert.h>
#include <nethost.h>
#include <coreclr_delegates.h>
#include <hostfxr.h>
#include <mb/pg_wchar.h> //For UTF8 support
#include <utils/numeric.h>
#include "plfsharp.h"

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

