################################################################################
lib.name = n_scopxy~
cflags = 
ldlibs = -lSDL2 -lGL -lGLU
class.sources = n_scopxy~.c
sources = \
./include/*
datafiles = \
n_scopxy~-help.pd \
n_scopxy~-meta.pd \
README.md \
LICENSE.txt

################################################################################
PDLIBBUILDER_DIR=pd-lib-builder/
include $(PDLIBBUILDER_DIR)/Makefile.pdlibbuilder
