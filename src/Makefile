TOP=..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS BELOW HERE

# Use c++11 atomic types and smart pointers (if c++11 is not supported by
# the compiler then we'll fall back on boost).
# If c++11 is enabled then see below for USE_PTHREADS.
USR_CXXFLAGS += -std=c++11

# USE_PTHREADS uses the pthread based mutex API and enables priority inheritance
# If USE_PTHREADS is undefined or 0 then c++ 11 mutexes are used (without
# priority inheritance). If c++11 is not enabled then USE_PTHREADS is enabled.
USR_CPPFLAGS += -DUSE_PTHREADS=1

# Build an IOC support library
LIBRARY_IOC += BsaCore

PROD_IOC += BsaTest
PROD_IOC += SyncBufTest

INC += BsaApi.h

# Compile and add the code to the support library
BsaCore_SRCS += BsaComp.cc
BsaCore_SRCS += BsaThread.cc
BsaCore_SRCS += BsaMutex.cc
BsaCore_SRCS += BsaCondVar.cc
BsaCore_SRCS += PatternBuffer.cc
BsaCore_SRCS += BsaChannel.cc
BsaCore_SRCS += BsaCore.cc
BsaCore_SRCS += BsaApi.cc
BsaCore_SRCS += BsaPosixClock.cc

BsaTest_SRCS += BsaTest.cc

BsaTest_SYS_LIBS += pthread mcheck

BsaTest_LIBS += BsaCore $(EPICS_BASE_IOC_LIBS)

SyncBufTest_SRC += SyncBufTest.cc
SyncBufTest_LIBS += BsaCore $(EPICS_BASE_IOC_LIBS)
SyncBufTest_SYS_LIBS += pthread

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD EXTRA GNUMAKE RULES BELOW HERE
