#!../../bin/linux-x86_64/germDaemon

#- You may have to change germDaemon to something else
#- everywhere it appears in this file

< envPaths

< unique.cmd

epicsEnvSet("NELM_MCA_GERM384", "1572864")
epicsEnvSet("NELM_TDC_GERM384", "393216")
epicsEnvSet("NELM_MCA_GERM192", "786432")
epicsEnvSet("NELM_TDC_GERM192", "196608")
epicsEnvSet("NELM_MCA_GERM96",  "393216")
epicsEnvSet("NELM_TDC_GERM96",  "98304")

cd $(TOP)

## Register all support components
dbLoadDatabase("dbd/germDaemon.dbd",0,0)
germDaemon_registerRecordDeviceDriver(pdbbase) 

## Load record instances

dbLoadRecords("db/germ.db","Sys=$(SYS), Dev=$(DEV), NELM_MCA=$(NELM_MCA_GERM${NELM}), NELM_TDC=$(NELM_TDC_GERM${NELM})")


iocInit()

dbl
