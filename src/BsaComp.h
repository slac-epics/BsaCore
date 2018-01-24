#ifndef BSA_COMP_H
#define BSA_COMP_H

#include <BsaApi.h>
#include <BsaTimeStamp.h>

typedef struct BsaVal {
	BsaTimeStamp  lastTs_;
	double        sum2_;
	double        mean_;
	unsigned long numSamples_;
	unsigned long missing_;
	BsaSevr       maxSevr_;
	BsaStat       maxSevrStat_;
} BsaVal;

class BsaComp {
private:
	BsaTimeStamp  initTs_;
	BsaVal        val_;

public:
	BsaComp();

	void          reset(BsaTimeStamp ts);

	void          resetAvg();

	void          addData(double x, BsaTimeStamp ts, BsaSevr sevr, BsaStat stat);

	void          miss();

	unsigned long getNum()         const;
	double        getMean()        const;
	double        getSumSquares()  const;
	// population variance
	double        getPopVar()      const;

	const BsaVal &getVal()         const;

	BsaTimeStamp  getTimeStamp()   const;

	unsigned long getMissing()     const;
 
	BsaSevr       getMaxSevr()     const;
	BsaStat       getMaxSevrStat() const;
};

#endif
