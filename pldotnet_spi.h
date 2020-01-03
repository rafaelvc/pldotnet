#ifndef _PLDOTNET_SPI_H_
	
#define _PLDOTNET_SPI_H_

#include "pldotnet_common.h"
	
extern int SPIExecute(char* cmd, long limit);
extern int SPIFetchResult (SPITupleTable *tuptable, int status);

typedef struct PropertyValue
{
    char   *value;
    char   *name;
    int    type;
}PropertyValue;
	
#endif

