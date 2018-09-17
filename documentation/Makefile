TOP=..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS BELOW HERE

DOCS+=BsaCore.pdf


include $(TOP)/configure/RULES
#----------------------------------------
#  ADD EXTRA GNUMAKE RULES BELOW HERE

BsaCore.pdf: ../BsaCore.tex $(TOP)/src/BsaApi.h fig_blkdiag.eps fig_impl.eps
	pdflatex $^
	pdflatex $^

fig_%.eps: ../fig_%.eps
	ln -s $^ $@

