TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#==================================================

# build a support library
LIBRARY_IOC += instronArby

DBD += instronArby.dbd

# specify all source files to be compiled and added to the library
instronArby_SRCS += drvAsynInstronArby.cpp

instronArby_LIBS += asyn
instronArby_LIBS += $(EPICS_BASE_IOC_LIBS)

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

