TOP=..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS BELOW HERE

USR_CXXFLAGS += -std=c++11

# Build an IOC support library
LIBRARY_IOC += BsaCore

# Compile and add the code to the support library
BsaCore_SRCS += BsaComp.cc
BsaCore_SRCS += PatternBuffer.cc

#BsaCore_LIBS += $(EPICS_BASE_IOC_LIBS)

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD EXTRA GNUMAKE RULES BELOW HERE

