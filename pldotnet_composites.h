/* 
 * PL/.NET (pldotnet) - PostgreSQL support for .NET C# and F# as 
 * 			procedural languages (PL)
 * 
 * 
 * Copyright 2019-2020 Brick Abode
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 * 
 * pldotnet_common.h
 *
 */
#ifndef PLDOTNETCOMPOSITES_H
#define PLDOTNETCOMPOSITES_H
#include  "pldotnet_common.h"

char * pldotnet_GetCompositeName(Oid oid);
int pldotnet_GetCompositeTypeSize(Oid oid);
int pldotnet_GetStructFromCompositeTuple(char * src, int src_size, Datum dat,
                                     Form_pg_type typeinfo, TupleDesc tupdesc);
int pldotnet_FillCompositeValues(char * cur_arg, Datum dat, Oid oid,
                                 FunctionCallInfo fcinfo, Form_pg_proc procst);
Datum pldotnet_CreateCompositeResult(char * composite_p, Oid oid,
                                                      FunctionCallInfo fcinfo);
#endif


