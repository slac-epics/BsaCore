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

	virtual void          reset(BsaTimeStamp ts);

	virtual void          resetAvg();

	virtual void          addData(double x, BsaTimeStamp ts, BsaSevr sevr, BsaStat stat);

	virtual void          miss();

	virtual unsigned long getNum()         const;
	virtual double        getMean()        const;
	virtual double        getSumSquares()  const;
	// population variance
	virtual double        getPopVar()      const;

	virtual BsaVal        getVal()         const;

	virtual BsaTimeStamp  getTimeStamp()   const;

	virtual unsigned long getMissing()     const;
 
	virtual BsaSevr       getMaxSevr()     const;
	virtual BsaStat       getMaxSevrStat() const;

	virtual ~BsaComp();
};

#endif
