# Makefile for PL/.NET

# General
DOTNET_INCDIR ?= /usr/include/dotnet  ## Fix here
DOTNETLIB ?= -L/usr/local/lib -ldotnet3.0 ## Fix here

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


# PG_CPPFLAGS = -I$(DOTNET_INCDIR) #-DP_DEBUG
# SHLIB_LINK = $(DOTNETLIB)

PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)

