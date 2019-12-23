#ifndef _PLDOTNET_SPI_H_
	
#define _PLDOTNET_SPI_H_
	

	
extern int SPIExecute(char* cmd, long limit);
	
extern int SPIFetchResult (SPITupleTable *tuptable, int status);
	

	
#endif

