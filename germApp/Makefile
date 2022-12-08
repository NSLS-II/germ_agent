TOP=..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

PROD_HOST += germ_getudpevents_spectra_padded
germ_getudpevents_spectra_padded_SRCS	  += germ_getudpevents_spectra_padded.c epics.c
germ_getudpevents_spectra_padded_LIBS	  += ezca
germ_getudpevents_spectra_padded_LIBS	  += $(EPICS_BASE_HOST_LIBS)
germ_getudpevents_spectra_padded_SYS_LIBS += pthread

PROD_HOST += test_agent
test_agent_SRCS     += test_agent.c
test_agent_LIBS	    += ezca
test_agent_LIBS     += $(EPICS_BASE_HOST_LIBS)
test_agent_SYS_LIBS += pthread

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

