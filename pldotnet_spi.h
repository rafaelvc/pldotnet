#ifndef _PLDOTNET_SPI_H_
	
#define _PLDOTNET_SPI_H_
	

	
extern unsigned long *SPIExecute(char* cmd, long limit);
extern unsigned long *SPIFetchResult (SPITupleTable *tuptable, int status);


typedef struct PropertyValue
{
    int    typesize_nullflags;
    char    typesize_params;
    int    typesize_result;
}PropertyValue;
	
#endif

