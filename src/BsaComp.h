#ifndef BSA_COMP_H
#define BSA_COMP_H

#include <BsaApi.h>
#include <BsaTimeStamp.h>

class BsaComp {
private:
	typedef BsaResultStruct *ResPtr;

	BsaTimeStamp  initTs_;
	ResPtr        current_;

public:
	BsaComp(ResPtr);

	void          reset(BsaTimeStamp ts, ResPtr r);

	void          resetAvg(ResPtr r);

	void          addData(double x, BsaTimeStamp ts, BsaPulseId, BsaSevr sevr, BsaStat stat);

	void          miss();

	unsigned long getNum()         const;
	double        getMean()        const;
	double        getSumSquares()  const;
	// population variance
	double        getPopVar()      const;

	BsaResult     getVal()         const;

	BsaTimeStamp  getTimeStamp()   const;

	unsigned long getMissing()     const;
 
	BsaSevr       getMaxSevr()     const;
	BsaStat       getMaxSevrStat() const;
};

#endif
