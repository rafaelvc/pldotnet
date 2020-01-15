#ifndef _PLDOTNET_SPI_H_

#define _PLDOTNET_SPI_H_

#include "pldotnet_common.h"

extern int SPIExecute(char* cmd, long limit);
extern int SPIFetchResult (SPITupleTable *tuptable, int status);

typedef struct PropertyValue
{
    unsigned long value;
    char   *name;
    int    type;
    int    nrow;
}PropertyValue;

#endif

