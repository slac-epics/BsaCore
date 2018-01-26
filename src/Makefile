TOP=..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS BELOW HERE

USR_CXXFLAGS += -std=c++11

# Build an IOC support library
LIBRARY_IOC += BsaCore

PROD_IOC += BsaTest
PROD_IOC += SyncBufTest

# Compile and add the code to the support library
BsaCore_SRCS += BsaComp.cc
BsaCore_SRCS += BsaThread.cc
BsaCore_SRCS += BsaMutex.cc
BsaCore_SRCS += BsaCondVar.cc
BsaCore_SRCS += PatternBuffer.cc
BsaCore_SRCS += BsaChannel.cc
BsaCore_SRCS += BsaCore.cc
BsaCore_SRCS += BsaApi.cc

BsaTest_SRCS += BsaTest.cc

BsaTest_SYS_LIBS += pthread

BsaTest_LIBS += BsaCore $(EPICS_BASE_IOC_LIBS)

SyncBufTest_SRC += SyncBufTest.cc
SyncBufTest_LIBS += BsaCore $(EPICS_BASE_IOC_LIBS)
SyncBufTest_SYS_LIBS += pthread

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD EXTRA GNUMAKE RULES BELOW HERE
