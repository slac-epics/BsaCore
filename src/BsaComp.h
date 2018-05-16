#ifndef BSA_COMP_H
#define BSA_COMP_H

#include <BsaApi.h>
#include <BsaTimeStamp.h>

class BsaComp {
private:
	typedef BsaResultStruct *ResPtr;

	ResPtr        current_;

public:
	BsaComp(ResPtr);

	void          resetAvg(ResPtr r);

	void          copy(ResPtr r);

	void          addData(double x, BsaTimeStamp ts, BsaPulseId, BsaSevr edefSevr, BsaSevr sevr, BsaStat stat);

	void          miss(BsaTimeStamp ts, BsaPulseId pid);

	unsigned long getCount()         const;

	BsaResult     getVal()           const;

	BsaTimeStamp  getTimeStamp()     const;

	unsigned long getMissing()       const;

	BsaSevr       getMaxSevr()       const;
	BsaStat       getMaxSevrStat()   const;
};

#endif
