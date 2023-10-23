#!../../bin/linux-x86_64/germDaemon

#- You may have to change germDaemon to something else
#- everywhere it appears in this file

< envPaths
#< /epics/common/xf27id1-ioc1-netsetup.cmd

#epicsEnvSet("SYS", "XF:27ID1-ES")
#epicsEnvSet("DEV", "{GeRM-Det:1}")
#epicsEnvSet("NELM", "192")
epicsEnvSet("SYS", "det")
epicsEnvSet("DEV", "1")
epicsEnvSet("NELM", "384")

epicsEnvSet("NELM_MCA_GERM384", "1572864")
epicsEnvSet("NELM_TDC_GERM384", "393216")
epicsEnvSet("NELM_MCA_GERM192", "786432")
epicsEnvSet("NELM_TDC_GERM192", "196608")
epicsEnvSet("NELM_MCA_GERM96",  "393216")
epicsEnvSet("NELM_TDC_GERM96",  "98304")

#epicsEnvSet("EPICS_CA_AUTO_ADDR_LIST",         "NO")
#epicsEnvSet("EPICS_CA_ADDR_LIST"     ,         "10.66.208.222")
#epicsEnvSet("EPICS_CAS_AUTO_BEACON_ADDR_LIST", "NO")
#epicsEnvSet("EPICS_CAS_BEACON_ADDR_LIST",      "10.66.208.222")
#epicsEnvSet("EPICS_CAS_INTF_ADDR_LIST",        "10.66.208.222")

epicsEnvSet("EPICS_CA_AUTO_ADDR_LIST",         "NO")
epicsEnvSet("EPICS_CA_ADDR_LIST"     ,         "172.16.0.255")
epicsEnvSet("EPICS_CAS_AUTO_BEACON_ADDR_LIST", "NO")
epicsEnvSet("EPICS_CAS_BEACON_ADDR_LIST",      "172.16.0.255")
epicsEnvSet("EPICS_CAS_INTF_ADDR_LIST",        "172.16.0.1")

cd $(TOP)

## Register all support components
dbLoadDatabase("dbd/germDaemon.dbd",0,0)
germDaemon_registerRecordDeviceDriver(pdbbase) 

## Load record instances

dbLoadRecords("db/germ.db","Sys=$(SYS), Dev=$(DEV), NELM_MCA=$(NELM_MCA_GERM${NELM}), NELM_TDC=$(NELM_TDC_GERM${NELM})")


iocInit()

dbl
