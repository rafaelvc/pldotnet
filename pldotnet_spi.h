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
 * pldotnet_spi.h
 *
 */

#ifndef _PLDOTNET_SPI_H_

#define _PLDOTNET_SPI_H_

#include "pldotnet_csharp.h"

extern int pldotnet_SPIExecute(char* cmd, long limit);
extern int pldotnet_SPIFetchResult (SPITupleTable *tuptable, int status);

typedef struct PropertyValue
{
    Datum value;
    char   *name;
    int    type;
    int    nrow;
}PropertyValue;

#endif

