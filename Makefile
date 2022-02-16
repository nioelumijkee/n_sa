################################################################################
lib.name = n_sa
cflags = 
ldlibs = -lSDL2
class.sources = n_sa~.c
sources = 
datafiles = \
n_sa~-help.pd \
n_sa~-meta.pd \
README.md \
LICENSE.txt

################################################################################
PDLIBBUILDER_DIR=pd-lib-builder/
include $(PDLIBBUILDER_DIR)/Makefile.pdlibbuilder
