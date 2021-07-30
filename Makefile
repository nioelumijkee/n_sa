################################################################################
lib.name = n_scop
cflags = 
ldlibs = -lSDL
class.sources = n_scop~.c
sources = 
datafiles = \
n_scop~-help.pd \
n_scop~-meta.pd \
README.md \
LICENSE.txt

################################################################################
PDLIBBUILDER_DIR=pd-lib-builder/
include $(PDLIBBUILDER_DIR)/Makefile.pdlibbuilder
