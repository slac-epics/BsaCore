#include <drvSup.h>
#include <epicsExport.h>
#include <BsaApi.h>
#include <errlog.h>

static long bsaCoreReport(int interest)
{
FILE *f = stdout;
	fprintf(f, "BSA Core:\n");
	BSA_DumpChannelStats(0, f);
	return 0;
}

static long bsaCoreInit()
{
	// lazy-init by C++
	return 0;
}

struct drvet drvBsaCore = {
	2,
	(DRVSUPFUN) bsaCoreReport,
	(DRVSUPFUN) bsaCoreInit
};

epicsExportAddress(drvet, drvBsaCore);
