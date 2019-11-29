#include "pldotnet_common.h"

bool
pldotnet_type_supported(Oid type)
{
    return (type == INT4OID || type == INT8OID
       || type == INT2OID   || type == FLOAT4OID
       || type == FLOAT8OID || type == VARCHAROID
       || type == BOOLOID   || type == TEXTOID
       || type == BPCHAROID || type == NUMERICOID);
}

const char *
pldotnet_getNetTypeName(Oid id, bool hasTypeConversion)
{
    switch (id){
        case BOOLOID:
            return "bool"; // System.Boolean
        case INT4OID:
            return "int"; // System.Int32
        case INT8OID:
            return "long"; // System.Int64
        case INT2OID:
            return "short"; // System.Int16
        case FLOAT4OID:
            return "float"; // System.Single
        case FLOAT8OID:
            return "double"; // System.Double
        case NUMERICOID:
            return hasTypeConversion ? "string" : "decimal"; // System.Decimal
        case BPCHAROID:
        case TEXTOID:
        case VARCHAROID:
            return "string"; // System.String
    }
    return "";
}

// Size in bytes
int
pldotnet_getTypeSize(Oid id)
{
    switch (id){
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

