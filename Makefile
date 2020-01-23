# Makefile for PL/.NET

# General
DOTNET_VER ?= 3.1.0
DOTNET_HOSTDIR ?= /usr/share/dotnet/shared/Microsoft.NETCore.App/$(DOTNET_VER)/
# TODO: Arch review. There is only .NET x64 flavours for Ubuntu/Debian Dec/2019
DOTNET_LIBDIR ?= /usr/share/dotnet/packs/Microsoft.NETCore.App.Host.linux-x64/$(DOTNET_VER)/runtimes/linux-x64/native/
DOTNET_INCHOSTDIR ?= $(DOTNET_HOSTDIR) $(shell env > /tmp/pgdotnet-make-env)
DOTNET_HOSTLIB ?= -L$(DOTNET_LIBDIR) -lnethost
PLNET_ENGINE_ROOT ?= /var/lib
PLNET_ENGINE_DIR := -D PLNET_ENGINE_DIR=$(PLNET_ENGINE_ROOT)/DotNetEngine

ifeq ("$(shell echo $(USE_DOTNETBUILD) | tr A-Z a-z)", "true")
	DEFINE_DOTNET_BUILD := -D USE_DOTNETBUILD
else
	GENERATE_BUILD_FILES := dotnet build $(PLNET_ENGINE_ROOT)/DotNetEngine/src/csharp
endif

PG_CONFIG ?= pg_config
PKG_LIBDIR := $(shell $(PG_CONFIG) --pkglibdir)

MODULE_big = pldotnet
EXTENSION = pldotnet
DATA = pldotnet--0.0.1.sql

REGRESS = \
	init-extension \
	testfunc \
	testintegers testnullintegers \
	testbool testnullbool \
	testnumeric \
	testfloats \
	testchar \
	testmixedtypes \
	testrecursive \
	testfsintegers \
	testspi \
	testarray \
	testcomposites \

OBJS = \
	pldotnet.o \
	pldotnet_common.o \
	pldotnet_hostfxr.o \
	pldotnet_csharp.o \
	pldotnet_fsharp.o \
	pldotnet_spi.o \
	pldotnet_composites.o
	#pldotnet_debug.o \

PG_CPPFLAGS = -I$(DOTNET_INCHOSTDIR) \
			  -Iinc -D LINUX $(DEFINE_DOTNET_BUILD) $(PLNET_ENGINE_DIR)

SHLIB_LINK = $(DOTNET_HOSTLIB)

PGXS := $(shell $(PG_CONFIG) --pgxs)

include $(PGXS)

plnet-install: install
	echo $(DOTNET_LIBDIR) > /etc/ld.so.conf.d/nethost_pldotnet.conf && ldconfig
	cp -r DotNetEngine $(PLNET_ENGINE_ROOT) && chown -R postgres $(PLNET_ENGINE_ROOT)/DotNetEngine
	sed -i 's/@PKG_LIBDIR/$(shell echo $(PKG_LIBDIR) | sed 's/\//\\\//g')/' $(PLNET_ENGINE_ROOT)/DotNetEngine/src/csharp/Engine.cs
	$(GENERATE_BUILD_FILES)

plnet-uninstall: uninstall
	rm -rf $(PLNET_ENGINE_ROOT)/DotNetEngine

plnet-install-dpkg:
	install -D -m 0755 -o postgres DotNetEngine/src/csharp/* -t $(DESTDIR)$(PLNET_ENGINE_ROOT)/DotNetEngine/src/csharp
	install -D -m 0755 -o postgres DotNetEngine/src/fsharp/* -t $(DESTDIR)$(PLNET_ENGINE_ROOT)/DotNetEngine/src/fsharp

