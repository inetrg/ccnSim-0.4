CCNSIM ?= src/ccnSim
INET_PATH ?= ../../inet
FLORA_PATH ?= ../../flora
INET_DSME_PATH ?= ../../inet-dsme
SIMULATION ?= ED_TTL-omnetpp
CONFIG ?= General

OPP_MAKEMAKE_KARGS = -KINET_PROJ=$(INET_PATH)
OPP_MAKEMAKE_DARGS = -DINET_IMPORT
OPP_MAKEMAKE_IARGS = -I../include -I$$\(INET_PROJ\)/src -I./packets
OPP_MAKEMAKE_LARGS = -L$$\(INET_PROJ\)/src
OPP_MAKEMAKE_DLARGS = -lINET$$\(D\)
OPP_MAKEMAKE_EXCLUDE = -X  ./patch/   -X scripts/ -X networks/ -X modules/  -o ccnSim -X results/ -X ini/ -X manual/  -X doc/ -X file_routing/ -X ccn14distrib/ -X ccn14scripts/ 

OPP_MAKEMAKE_ARGS += -f --deep -o ccnSim
OPP_MAKEMAKE_ARGS += $(OPP_MAKEMAKE_KARGS)
OPP_MAKEMAKE_ARGS += $(OPP_MAKEMAKE_DARGS)
OPP_MAKEMAKE_ARGS += $(OPP_MAKEMAKE_IARGS)
OPP_MAKEMAKE_ARGS += $(OPP_MAKEMAKE_LARGS)
OPP_MAKEMAKE_ARGS += $(OPP_MAKEMAKE_DLARGS)
OPP_MAKEMAKE_ARGS += '$$(INET_DSME_OBJS_LIST)'

#INET_LIB = $(INET_PATH)/src/INET
#INET_DSME_LIB = $(INET_DSME_PATH)/src/inet-dsme
#FLORA_LIB = $(FLORA_PATH)/src/flora

#NED_INCLUDES = ../src:.:$(INET_PATH)/src:$(INET_PATH)/examples:$(INET_PATH)/tutorials:$(INET_PATH)/showcases:$(INET_DSME_PATH)/src:$(FLORA_PATH)/src

CMDENV ?= 0
VERBOSE ?= 0

ifneq (0, $(CMDENV))
  OMNETPP_EXTRA_ARGS += -u Cmdenv
  ifneq (0, $(VERBOSE))
	OMNETPP_EXTRA_ARGS += --cmdenv-express-mode=false --cmdenv-log-prefix="[%l] %m: %|"
  endif
endif

define CCNSIM_ARGS
-m $(OMNETPP_EXTRA_ARGS)
endef

BUILD_LIB ?= 0
BUILD_STATIC_LIB ?= 0

ifneq (0, $(BUILD_LIB))
  OPP_MAKEMAKE_ARGS += -s
endif

ifneq (0, $(BUILD_STATIC_LIB))
  OPP_MAKEMAKE_ARGS += -a
endif

CCNSIM_ARGS += $(SIMULATION).ini -c $(CONFIG)

all: checkmakefiles
	cd src && $(MAKE)

clean: checkmakefiles
	cd src && $(MAKE) clean

cleanall: checkmakefiles
	cd src && $(MAKE) MODE=release clean
	cd src && $(MAKE) MODE=debug clean
	rm -f src/Makefile

makefiles:
	cd src && opp_makemake $(OPP_MAKEMAKE_ARGS)


checkmakefiles:
	@if [ ! -f src/Makefile ]; then \
	echo; \
	echo '======================================================================='; \
	echo 'src/Makefile does not exist. Please use "make makefiles" to generate it!'; \
	echo '======================================================================='; \
	echo; \
	exit 1; \
	fi

run:
	$(CCNSIM) $(CCNSIM_ARGS)

debug:
	$(CCNSIM)_dbg $(CCNSIM_ARGS)

gdb:
	gdb --args $(CCNSIM)_dbg $(CCNSIM_ARGS)
