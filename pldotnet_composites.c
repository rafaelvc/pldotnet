#include  "pldotnet_composites.h"
#include  "pldotnet_csharp.h"

/*
 *   Builds a C# struct in the source from a composite like: 
 *
 *   [StructLayout(LayoutKind.Sequential,Pack=1)]
 *   public struct CompositeN;
 *   {
 *       public fielType1 fieldName1;
 *       public fielType2 fieldName2;
 *       ...
 *       public fielTypeN fieldNameN;
 *   }
*/

int
pldotnet_BuildCSharpStructFromTuple(char * src, int src_size, Datum dat, int nr, 
                                                                        Oid oid)
{
    const char composite_header[] = \
"[StructLayout(LayoutKind.Sequential,Pack=1)]\n\
public struct ";
    const char composite_name[] = \
              "Composite";
    const char composite_start[] = \
"{";
    const char composite_end[] = \
"}"; 
    const char semicon[] = ";";

    int cursize = 0;

    Form_pg_type typeinfo;
    HeapTuple type;
    TupleDesc tupdesc;
    HeapTupleHeader tup;

    const char *key;
    bool isnull;
    //Datum value;
    Oid type_attr;

    type = SearchSysCache(TYPEOID, ObjectIdGetDatum(oid), 0, 0, 0);
    if (!HeapTupleIsValid(type))
      elog(ERROR, "[pldotnet]: cache lookup failed for type %u", oid);

    typeinfo = (Form_pg_type) GETSTRUCT(type);
    tupdesc = lookup_rowtype_tupdesc(oid, typeinfo->typtypmod);

    tup = DatumGetHeapTupleHeader(dat);

    SNPRINTF(src, src_size, "%s%s%d%s", composite_header, composite_name, nr,
                                                               composite_start);
    cursize = strlen(src);
    src_size -= cursize;
    src += cursize;

    for (int i = 0; i < tupdesc->natts; i++) 
    {
        key = NameStr(TupleDescAttr(tupdesc, i)->attname);
        //value = GetAttributeByNum(tup, TupleDescAttr(tupdesc, i)->attnum,
        GetAttributeByNum(tup, TupleDescAttr(tupdesc, i)->attnum, &isnull);
        type_attr = TupleDescAttr(tupdesc, i)->atttypid;
        if (!isnull) 
        {
            SNPRINTF(src, src_size,"%s%s%s%s"
                        , pldotnet_PublicDecl(type_attr)
                        , pldotnet_GetNetTypeName(type_attr , true)
                        , key, semicon);
            cursize = strlen(src);
            src_size -= cursize;
            src += cursize;
        }
    }
    SNPRINTF(src, src_size, "%s", composite_end);

    ReleaseSysCache(type);   

    return 0;
}


