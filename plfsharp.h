#ifndef PLFSHARP_H
#define PLFSHARP_H

char * plfsharp_build_block2(Form_pg_proc procst);
char * plfsharp_build_block4(Form_pg_proc procst, HeapTuple proc);
char * plfsharp_build_block6(Form_pg_proc procst);
char * plfsharp_CreateCStrucLibArgs(FunctionCallInfo fcinfo, Form_pg_proc procst);
Datum plfsharp_getResultFromDotNet(char * libArgs, Oid rettype, FunctionCallInfo fcinfo);

#endif  /* PLFSHARP_H */
