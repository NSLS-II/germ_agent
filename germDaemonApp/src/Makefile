TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#=============================
# Build the IOC application

PROD_IOC = germDaemon
# germDaemon.dbd will be created and installed
DBD += germDaemon.dbd

# germDaemon.dbd will be made up from these files:
germDaemon_DBD += base.dbd

# germDaemon_registerRecordDeviceDriver.cpp derives from germDaemon.dbd
germDaemon_SRCS += germDaemon_registerRecordDeviceDriver.cpp

# Build the main IOC entry point on workstation OSs.
germDaemon_SRCS_DEFAULT += germDaemonMain.cpp
germDaemon_SRCS_vxWorks += -nil-


# Finally link to the EPICS Base libraries
germDaemon_LIBS += $(EPICS_BASE_IOC_LIBS)


# Germanium daemon

PROD_HOST += germ_daemon
germ_daemon_SRCS     += germ.c udp_conn.c data_proc.c exp_mon.c 
germ_daemon_LIBS     += $(EPICS_BASE_HOST_LIBS)
germ_daemon_SYS_LIBS += pthread


#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

