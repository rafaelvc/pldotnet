#ifndef PLNETHOST_H
#define PLNETHOST_H

#include <nethost.h>
// Header files copied from https://github.com/dotnet/core-setup
#include <coreclr_delegates.h>
#include <hostfxr.h>

hostfxr_initialize_for_runtime_config_fn init_fptr;
hostfxr_get_runtime_delegate_fn get_delegate_fptr;
hostfxr_close_fn close_fptr;

int pldotnet_load_hostfxr();
load_assembly_and_get_function_pointer_fn get_dotnet_load_assembly(const char_t *assembly);
/* loaded host placeholder variable */
void *nethost_lib;

#endif  /* PLNETHOST_H */
