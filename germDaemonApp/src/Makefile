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

#============================================

# Build the main IOC entry point on workstation OSs.
germDaemon_SRCS_DEFAULT += germDaemonMain.cpp
germDaemon_SRCS_vxWorks += -nil-
germDaemon_LIBS += $(EPICS_BASE_IOC_LIBS)


#============================================

# Multi-thread Germanium daemon
PROD_HOST += germ_daemon
#germ_daemon_SRCS     += germ.c udp_conn.c data_write.c data_proc.c exp_mon.c 
germ_daemon_SRCS     += germ_atom.c udp_conn_atom.c data_write_atom.c data_proc_atom.c exp_mon.c mover.c
germ_daemon_LIBS     += $(EPICS_BASE_HOST_LIBS)
germ_daemon_SYS_LIBS += pthread

#============================================

## Original single thread UDP daemon
#PROD_HOST += germ_udp
#germ_udp_SRCS += germ_getudpevents.c

##============================================
#
## Single thread UDP daemon with spectra calculation
#PROD_HOST += germ_udp_spectra
#germ_udp_spectra_SRCS += germ_getudpevents_spectra_padded.c
#germ_udp_spectra_LIBS += ezca
#
##============================================
#
## Single thread UDP daemon with spectra calculation with variable nelm
#PROD_HOST += germ_udp_spectra_varnelm
#germ_udp_spectra_varnelm_SRCS += germ_getudpevents_spectra_padded_varnelm.c
#germ_udp_spectra_varnelm_LIBS += ezca

#============================================

## To be called from CSS screen
#PROD_HOST += testenable
#testenable_SRCS += testenable.c
#testenable_LIBS += ezca
#testenable_LIBS += $(EPICS_BASE_HOST_LIBS)
#
#PROD_HOST += testdisable
#testdisable_SRCS += testdisable.c
#testdisable_LIBS += ezca
#testdisable_LIBS += $(EPICS_BASE_HOST_LIBS)
#
#PROD_HOST += testallon
#testallon_SRCS  += testallon.c
#testallon_LIBS  += ezca
#testallon_LIBS  += $(EPICS_BASE_HOST_LIBS)
#
#PROD_HOST += testalloff
#testalloff_SRCS += testalloff.c
#testalloff_LIBS += ezca
#testalloff_LIBS += $(EPICS_BASE_HOST_LIBS)
#
#PROD_HOST += enablealloff
#enablealloff_SRCS += enablealloff.c
#enablealloff_LIBS += ezca
#enablealloff_LIBS += $(EPICS_BASE_HOST_LIBS)
#
#PROD_HOST += enableallon
#enableallon_SRCS  += enableallon.c
#enableallon_LIBS  += ezca
#enableallon_LIBS  += $(EPICS_BASE_HOST_LIBS)
#
#PROD_HOST += chandisable
#chandisable_SRCS  += chandisable.c
#chandisable_LIBS  += ezca
#chandisable_LIBS  += $(EPICS_BASE_HOST_LIBS)
#
#PROD_HOST += chanenable
#chanenable_SRCS += chanenable.c
#chanenable_LIBS += ezca
#chanenable_LIBS += $(EPICS_BASE_HOST_LIBS)

#============================================

#CXXFLAGS += -g
germ_daemon_CFLAGS += -g

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

