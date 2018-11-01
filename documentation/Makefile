TOP=..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS BELOW HERE

DOCS+=../BsaCore.pdf

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD EXTRA GNUMAKE RULES BELOW HERE

ifeq ($(WITH_DOCS),YES)
../BsaCore.pdf: ../BsaCore.tex $(TOP)/src/BsaApi.h $(patsubst ../%,%,$(wildcard ../fig_*.eps))
	$(RM) $@
	pdflatex $^
	pdflatex $^
	cp BsaCore.pdf ../
endif


fig_%.eps: ../fig_%.eps
	ln -s $^ $@
