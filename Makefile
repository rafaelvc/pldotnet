# Makefile for PL/.NET

# General
DOTNET_HOSTDIR ?= $(shell find / -path "*/native/nethost.h" 2> /dev/null | sed 's/\/nethost\.h//g')
DOTNET_INCHOSTDIR ?= $(DOTNET_HOSTDIR)
DOTNET_HOSTLIB ?= -L$(DOTNET_HOSTDIR) -lnethost
PLNET_ENGINE_ROOT ?= /var/lib
PLNET_ENGINE_DIR := -D PLNET_ENGINE_DIR=$(PLNET_ENGINE_ROOT)/DotNetLib

ifeq ("$(shell echo $(USE_DOTNETBUILD) | tr A-Z a-z)", "true")
	DEFINE_DOTNET_BUILD := -D USE_DOTNETBUILD
else
	GENERATE_BUILD_FILES := dotnet build $(PLNET_ENGINE_ROOT)/DotNetLib/src/csharp
endif

PG_CONFIG ?= pg_config
PKG_LIBDIR := $(shell $(PG_CONFIG) --pkglibdir)



MODULE_big = pldotnet
EXTENSION = pldotnet
DATA = pldotnet--0.0.1.sql
#DATA_built = pldotnet.sql

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
testfsintegers

OBJS = \
pldotnet.o \
pldotnet_common.o \
pldotnet_hostfxr.o \
pldotnet_csharp.o \
pldotnet_fsharp.o
#pldotnet_debug.o \

PG_CPPFLAGS = -I$(DOTNET_INCHOSTDIR) \
			  -Iinc -D LINUX $(DEFINE_DOTNET_BUILD) $(PLNET_ENGINE_DIR) \
			  -g -Wl,-rpath,'$$ORIGIN',--disable-new-dtags

SHLIB_LINK = $(DOTNET_HOSTLIB)

PGXS := $(shell $(PG_CONFIG) --pgxs)

include $(PGXS)

plnet-install: install
	echo "$$(find / -path "*/native/nethost.h" | sed 's/\/nethost\.h//g' 2> /dev/null)"  > /etc/ld.so.conf.d/nethost.conf && ldconfig
	install -D -m 0755 -o postgres DotNetLib/src/csharp/* -t $(DESTDIR)$(PLNET_ENGINE_ROOT)/DotNetLib/src/csharp
	install -D -m 0755 -o postgres DotNetLib/src/fsharp/* -t $(DESTDIR)$(PLNET_ENGINE_ROOT)/DotNetLib/src/fsharp
	$(GENERATE_BUILD_FILES)

plnet-uninstall: uninstall
	rm -rf $(PLNET_ENGINE_ROOT)/DotNetLib
