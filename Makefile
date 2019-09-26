# Makefile for PL/.NET

# General
DOTNET_HOSTDIR ?= $(shell find / -path "*/native/nethost.h" | sed 's/\/nethost\.h//g')
DOTNET_INCHOSTDIR ?= $(DOTNET_HOSTDIR)
#DOTNETLIB ?= -L/usr/local/lib -ldotnet3.0 ## Fix here
DOTNET_HOSTLIB ?= -L$DOTNET_HOSTDIR -ldotnethost

PG_CONFIG ?= pg_config
PKG_LIBDIR := $(shell $(PG_CONFIG) --pkglibdir)

MODULE_big = pldotnet
EXTENSION = pldotnet
DATA = pldotnet--0.0.1.sql
#DATA_built = pldotnet.sql

# REGRESS = \
# pldotnettest \

OBJS = \
pldotnet.o \
#pldotnet_debug.o \

PG_CPPFLAGS = -I$(DOTNET_INCHOSTDIR) -Iinc -D LINUX -g -Wl,-rpath,'$$ORIGIN',--disable-new-dtags
SHLIB_LINK = $(DOTNET_HOSTLIB)

PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)

