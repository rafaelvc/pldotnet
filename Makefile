# Makefile for PL/.NET

# General
DOTNET_HOSTDIR ?= $(shell find / -path "*/native/nethost.h" | sed 's/\/nethost\.h//g')
DOTNET_INCHOSTDIR ?= $(DOTNET_HOSTDIR)
DOTNET_HOSTLIB ?= -L$(DOTNET_HOSTDIR) -lnethost
DOTNET_ROOT_DIR ?= /var/lib
DOTNET_SRC_DIR := -D DOTNET_SRC_DIR=$(DOTNET_ROOT_DIR)/DotNetLib

ifeq ("$(shell echo $(USE_DOTNETBUILD) | tr A-Z a-z)", "true")
	DEFINE_DOTNET_BUILD := -D USE_DOTNETBUILD
else
	GENERATE_BUILD_FILES := dotnet build $(DOTNET_ROOT_DIR)/DotNetLib/src
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

OBJS = \
pldotnet.o \
#pldotnet_debug.o \

PG_CPPFLAGS = -I$(DOTNET_INCHOSTDIR) \
			  -Iinc -D LINUX $(DEFINE_DOTNET_BUILD) $(DOTNET_SRC_DIR) \
			  -g -Wl,-rpath,'$$ORIGIN',--disable-new-dtags

SHLIB_LINK = $(DOTNET_HOSTLIB)

PGXS := $(shell $(PG_CONFIG) --pgxs)

include $(PGXS)

plnet-install: install
	echo "$$(find / -path "*/native/nethost.h" | sed 's/\/nethost\.h//g' 2> /dev/null)"  > /etc/ld.so.conf.d/nethost.conf && ldconfig
	cp -r DotNetLib $(DOTNET_ROOT_DIR) && chown -R postgres $(DOTNET_ROOT_DIR)/DotNetLib
	$(GENERATE_BUILD_FILES)

plnet-uninstall: uninstall
	rm -rf $(DOTNET_ROOT_DIR)/DotNetLib
