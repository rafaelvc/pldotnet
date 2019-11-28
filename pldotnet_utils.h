#ifndef PLDOTNET_UTILS_H
#define PLDOTNET_UTILS_H

typedef struct pldotnet_info
{
    char * libArgs;
    int    typeSizeNullFlags;
    int    typeSizeOfParams;
    int    typeSizeOfResult;
}pldotnet_info;
pldotnet_info dotnet_info;

bool pldotnet_type_supported(Oid type);
const char * pldotnet_getNetTypeName(Oid id, bool hasTypeConversion);
int pldotnet_getTypeSize(Oid id);

#endif  /* PLDOTNET_UTILS_H */
