#!/bin/bash
if [[ (${BUILD_LIB} == 1) ]]; then
    OPP_MAKEMAKE_ARGS=-s
fi
opp_makemake --deep -f -X  ./patch/   -X scripts/ -X networks/ -X modules/  -o ccnSim -X results/ -X ini/ -X manual/  -X doc/ -X file_routing/ -X ccn14distrib/ -X ccn14scripts/ -I include/ -I packets/ -I ../inet/src ${OPP_MAKEMAKE_ARGS}
