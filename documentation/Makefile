TOP=..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS BELOW HERE

ifndef TEXINPUTS
ifneq (,$(wildcard /afs/slac/g/lcls/tools/texmf))
export TEXINPUTS="/afs/slac/g/lcls/tools/texmf:"
endif
endif

ifdef TEXINPUTS
DOCS+=BsaCore.pdf
endif


include $(TOP)/configure/RULES
#----------------------------------------
#  ADD EXTRA GNUMAKE RULES BELOW HERE

BsaCore.pdf: ../BsaCore.tex $(TOP)/src/BsaApi.h $(patsubst ../%,%,$(wildcard ../fig_*.eps))
	pdflatex $^
	pdflatex $^

fig_%.eps: ../fig_%.eps
	ln -s $^ $@

