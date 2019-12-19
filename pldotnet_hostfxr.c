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
 * pldotnet_hostfxr.c - .NET Hostfxr and interop functions
 *
 */ 
/******************************************************************************
 * Functions used to load and activate .NET Core
 *****************************************************************************/
#include <postgres.h>
#include <assert.h>
#include <dlfcn.h>
#include <limits.h>
#include "pldotnet_hostfxr.h"

#define nullptr ((void*)0)
#define MAX_PATH PATH_MAX

/* Forward declarations */
static void *pldotnet_LoadLibrary(const char_t *);
static void *pldotnet_GetExport(void *, const char *);

static hostfxr_initialize_for_runtime_config_fn init_fptr;
static hostfxr_get_runtime_delegate_fn get_delegate_fptr;
static hostfxr_close_fn close_fptr;

/* Implementations */
static void *
pldotnet_LoadLibrary(const char_t *path)
{
    fprintf(stderr, "# DEBUG: doing dlopen(%s).\n", path);
    nethost_lib = dlopen(path, RTLD_LAZY | RTLD_LOCAL);
    assert(nethost_lib != nullptr);
    return nethost_lib;
}

static void *
pldotnet_GetExport(void *host, const char *name)
{
    void *f = dlsym(host, name);
    if (f == nullptr)
    {
        fprintf(stderr, "Can't dlsym(%s); exiting.\n", name);
        exit(-1);
    }
    return f;
}

/* Using the nethost library, discover the location of hostfxr and get exports */
int
pldotnet_LoadHostfxr()
{
    /* Pre-allocate a large buffer for the path to hostfxr */
    char_t buffer[MAX_PATH];
    size_t buffer_size = sizeof(buffer) / sizeof(char_t);
    int rc = get_hostfxr_path(buffer, &buffer_size, nullptr);
    void *lib;
    if (rc != 0)
        return 0;

    /* Load hostfxr and get desired exports */
    lib = pldotnet_LoadLibrary(buffer);
    init_fptr = (hostfxr_initialize_for_runtime_config_fn)pldotnet_GetExport( \
        lib, "hostfxr_initialize_for_runtime_config");
    get_delegate_fptr = (hostfxr_get_runtime_delegate_fn)pldotnet_GetExport( \
        lib, "hostfxr_get_runtime_delegate");
    close_fptr = (hostfxr_close_fn)pldotnet_GetExport(lib, "hostfxr_close");

    return (init_fptr && get_delegate_fptr && close_fptr);
}

/* Load and initialize .NET Core and get desired function pointer for scenario */
load_assembly_and_get_function_pointer_fn
GetNetLoadAssembly(const char_t *config_path)
{
    /* Load .NET Core */
    void *load_assembly_and_get_function_pointer = nullptr;
    hostfxr_handle cxt = nullptr;
    int rc = init_fptr(config_path, nullptr, &cxt);
    if (rc > 1 || rc < 0 || cxt == nullptr)
    {
        fprintf(stderr, "Init failed: %x\n", rc);
        close_fptr(cxt);
        return nullptr;
    }

    /* Get the load assembly function pointer */
    rc = get_delegate_fptr(
        cxt,
        hdt_load_assembly_and_get_function_pointer,
        &load_assembly_and_get_function_pointer);
    if (rc != 0 || load_assembly_and_get_function_pointer == nullptr)
        fprintf(stderr, "Get delegate failed: %x\n", rc);
    close_fptr(cxt);
    return (load_assembly_and_get_function_pointer_fn)load_assembly_and_get_function_pointer;
}

