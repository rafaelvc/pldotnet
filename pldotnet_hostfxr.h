/*
 * Work in this file comes from:
 * (https://github.com/dotnet/samples/tree/master/core/hosting/HostWithHostFxr)
 *
 * Copyright and LICENSE is accodring to what is found there:
 *
 * Licensed to the .NET Foundation under one or more agreements.
 * The .NET Foundation licenses this file to you under the MIT license.
 * See the LICENSE file of (https://github.com/dotnet/samples)
 * for more information.
 *
 * pldotnet_hostfxr.h
 *
 */
#ifndef PLNETHOST_H
#define PLNETHOST_H

#include <nethost.h>
/* Header files copied from https://github.com/dotnet/core-setup */
#include <coreclr_delegates.h>
#include <hostfxr.h>

int pldotnet_LoadHostfxr(void);
load_assembly_and_get_function_pointer_fn GetNetLoadAssembly(const char_t *assembly);
/* loaded host placeholder variable */
void *nethost_lib;

#endif  /* PLNETHOST_H */
