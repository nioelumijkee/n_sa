################################################################################
lib.name = n_sxy
cflags = 
ldlibs = -lSDL2 -lGL -lGLU
class.sources = n_sxy~.c
sources = 
datafiles = \
n_sxy~-help.pd \
n_sxy~-meta.pd \
README.md \
LICENSE.txt

################################################################################
PDLIBBUILDER_DIR=pd-lib-builder/
include $(PDLIBBUILDER_DIR)/Makefile.pdlibbuilder
