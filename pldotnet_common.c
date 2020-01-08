/* 
 * PL/.NET (pldotnet) - PostgreSQL support for .NET C# and F# as 
 *                      procedural languages (PL)
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
 * pldotnet_common.c - Common functions for PG <-> .NET type delivering
 *
 */
#include "pldotnet_common.h"
#include "pldotnet_composites.h"

bool
pldotnet_TypeSupported(Oid type)
{
    return (type == INT4OID || type == INT8OID
       || type == INT2OID   || type == FLOAT4OID
       || type == FLOAT8OID || type == VARCHAROID
       || type == BOOLOID   || type == TEXTOID
       || type == BPCHAROID || type == NUMERICOID || TYPTYPE_COMPOSITE);
}

const char *
pldotnet_GetNetTypeName(Oid id, bool hastypeconversion)
{
    switch (id)
    {
        case BOOLOID:
            return "bool";   /* System.Boolean */
        case INT4OID:
            return "int";    /* System.Int32 */
        case INT8OID:
            return "long";   /* System.Int64 */
        case INT2OID:
            return "short";  /* System.Int16 */
        case FLOAT4OID:
            return "float";  /* System.Single */
        case FLOAT8OID:
            return "double"; /* System.Double */
        case NUMERICOID:     /* System.Decimal */
            return hastypeconversion ? "string" : "decimal";
        case BPCHAROID:
        case TEXTOID:
        case VARCHAROID:
            return "string"; /* System.String */
        case TYPTYPE_COMPOSITE:
            return pldotnet_GetCompositeName(id);
    }
    return "";
}

/* Native type size in bytes */
int
pldotnet_GetTypeSize(Oid id)
{
    switch (id)
    {
        case BOOLOID:
            return sizeof(bool);
        case INT4OID:
            return sizeof(int);
        case INT8OID:
            return sizeof(long);
        case INT2OID:
            return sizeof(short);
        case FLOAT4OID:
            return sizeof(float);
        case FLOAT8OID:
            return sizeof(double);
        case NUMERICOID:
        case BPCHAROID:
        case TEXTOID:
        case VARCHAROID:
            return sizeof(char *);
    }
    return -1;
}

