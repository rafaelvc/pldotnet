#include "pldotnet_composites.h"
#include "pldotnet_csharp.h"

/*
 *   Builds a C# struct in the source from a composite like: 
 *
 *   [StructLayout(LayoutKind.Sequential,Pack=1)]
 *   public struct CompositeName1;
 *   {
 *       public fielType1 fieldName1;
 *       public fielType2 fieldName2;
 *       ...
 *       public fielTypeN fieldNameN;
 *   }
*/

int
pldotnet_GetStructFromCompositeTuple(char * src, int src_size, Datum dat,
                                       Form_pg_type typeinfo, TupleDesc tupdesc)
{
    const char composite_header[] = \
"[StructLayout(LayoutKind.Sequential,Pack=1)]\n\
public struct ";
    const char composite_start[] = \
"\n{";
    const char composite_end[] = \
"\n}";
    const char semicon[] = ";";
    int cursize = 0;

    HeapTupleHeader tup;

    const char *key;
    bool isnull;
    Oid type_attr;

    tup = DatumGetHeapTupleHeader(dat);

    SNPRINTF(src, src_size, "%s%s%s", composite_header,
                          NameStr( typeinfo->typname ), composite_start);
    cursize = strlen(src);
    src_size -= cursize;
    src += cursize;

    for (int i = 0; i < tupdesc->natts; i++) 
    {
        key = NameStr(TupleDescAttr(tupdesc, i)->attname);
        GetAttributeByNum(tup, TupleDescAttr(tupdesc, i)->attnum, &isnull);
        type_attr = TupleDescAttr(tupdesc, i)->atttypid;
        if (!isnull) 
        {
            SNPRINTF(src, src_size,"%s%s %s%s"
                        , pldotnet_PublicDecl(type_attr)
                        , pldotnet_GetNetTypeName(type_attr , true)
                        , key, semicon);
            cursize = strlen(src);
            src_size -= cursize;
            src += cursize;
        }
    }
    SNPRINTF(src, src_size, "%s", composite_end);

    return 0;
}

char * 
pldotnet_GetCompositeName(Oid oid)
{
    char * nm;
    HeapTuple type;
    Form_pg_type typeinfo;
    type = SearchSysCache(TYPEOID, ObjectIdGetDatum(oid), 0, 0, 0);
    if (!HeapTupleIsValid(type))
        elog(ERROR, "[pldotnet]: cache lookup failed for type %u", oid);
    typeinfo = (Form_pg_type) GETSTRUCT(type);
    nm = NameStr( typeinfo->typname );
    ReleaseSysCache(type);
    return nm;
}

int 
pldotnet_GetCompositeTypeSize(Oid oid)
{
    int typtotal_size = 0;
    Form_pg_type typeinfo;
    HeapTuple type;
    TupleDesc tupdesc;
    Oid type_attr;
    type = SearchSysCache(TYPEOID, ObjectIdGetDatum(oid), 0, 0, 0);
    if (!HeapTupleIsValid(type))
      elog(ERROR, "[pldotnet]: cache lookup failed for type %u", oid);
    typeinfo = (Form_pg_type) GETSTRUCT(type);
    if (typeinfo->typtype != TYPTYPE_COMPOSITE)
    {
        ReleaseSysCache(type);
        return -1;
    }
    tupdesc = lookup_rowtype_tupdesc(oid, typeinfo->typtypmod);
    for (int i = 0; i < tupdesc->natts; i++)
    {
        type_attr = TupleDescAttr(tupdesc, i)->atttypid;
        typtotal_size += pldotnet_GetTypeSize(type_attr);
    }
    ReleaseTupleDesc(tupdesc);
    ReleaseSysCache(type);
    return typtotal_size;
}


int 
pldotnet_FillCompositeValues(char * cur_arg, Datum dat, Oid oid, 
                                  FunctionCallInfo fcinfo, Form_pg_proc procst)
{
    Form_pg_type typeinfo;
    HeapTuple type;
    TupleDesc tupdesc;
    HeapTupleHeader tup;

    bool isnull;
    Datum value;
    Oid type_attr;

    type = SearchSysCache(TYPEOID, ObjectIdGetDatum(oid), 0, 0, 0);
    if (!HeapTupleIsValid(type))
      elog(ERROR, "[pldotnet]: cache lookup failed for type %u", oid);

    typeinfo = (Form_pg_type) GETSTRUCT(type);
    if (typeinfo->typtype != TYPTYPE_COMPOSITE)
    {
        ReleaseSysCache(type);
        return -1;
    }

    tupdesc = lookup_rowtype_tupdesc(oid, typeinfo->typtypmod);
    tup = DatumGetHeapTupleHeader(dat);

    for (int i = 0; i < tupdesc->natts; i++) 
    {
        type_attr = TupleDescAttr(tupdesc, i)->atttypid;
        value = GetAttributeByNum(tup, TupleDescAttr(tupdesc, i)->attnum,
                                                                       &isnull);
        pldotnet_SetScalarValue(cur_arg, value, fcinfo, i, type_attr, &isnull);
        cur_arg += pldotnet_GetTypeSize(type_attr);
    }
    ReleaseTupleDesc(tupdesc);
    ReleaseSysCache(type);
    return 0;
}

