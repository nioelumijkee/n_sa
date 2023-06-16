################################################################################
lib.name = n_sa
cflags = 
ldlibs = -lSDL2 -lGL -lGLU
class.sources = n_sa~.c n_sxy~.c
sources = 

################################################################################
PDLIBBUILDER_DIR=pd-lib-builder/
include $(PDLIBBUILDER_DIR)/Makefile.pdlibbuilder
