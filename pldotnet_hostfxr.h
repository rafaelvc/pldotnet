#ifndef PLNETHOST_H
#define PLNETHOST_H

#include <nethost.h>
// Header files copied from https://github.com/dotnet/core-setup
#include <coreclr_delegates.h>
#include <hostfxr.h>

int pldotnet_load_hostfxr();
load_assembly_and_get_function_pointer_fn get_dotnet_load_assembly(const char_t *assembly);
/* loaded host placeholder variable */
void *nethost_lib;

#endif  /* PLNETHOST_H */
