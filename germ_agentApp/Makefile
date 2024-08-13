TOP=..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#============================================

# Multi-thread Germanium daemon
PROD_HOST += germ_daemon
germ_daemon_SRCS     += germ.c udp_conn.c data_write.c exp_mon.c 
germ_daemon_LIBS     += $(EPICS_BASE_HOST_LIBS)
germ_daemon_SYS_LIBS += pthread

#============================================
#CXXFLAGS += -g
germ_daemon_CFLAGS += -g

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

