#ifndef BSA_CORE_FACTORY_H
#define BSA_CORE_FACTORY_H

#include <BsaCore.h>
#include <BsaAlias.h>

typedef BsaAlias::shared_ptr<BsaCore> BsaCorePtr;

class BsaCoreFactory {
public:
	static const unsigned DEFAULT_BSA_LD_PATTERNBUF_SZ  = 9;
	// cannot legally initialize a 'double' const member here...
	static const unsigned DEFAULT_BSA_UPDATE_TIMEOUT_MS = 300;

private:
	unsigned ldBufSz_;
	unsigned minfill_;
	int      patternBufPriority_;
	int      inputBufPriority_;
	int      outputBufPriority_;
	double   updateTimeout_;

public:
	BsaCoreFactory();

	BsaCoreFactory & setLdBufSz(unsigned);
	BsaCoreFactory & setMinFill(unsigned);
	BsaCoreFactory & setPatternBufPriority(int);
	BsaCoreFactory & setInputBufPriority(int);
	BsaCoreFactory & setOutputBufPriority(int);
	BsaCoreFactory & setUpdateTimeoutSecs(double);

	unsigned         getLdBufSz()
	{
		return ldBufSz_;
	}

	unsigned         getMinFill()
	{
		return minfill_ ? minfill_ : ( 1<< ( ldBufSz_ - 1 ) );
	}

	int              getPatternBufPriority()
	{
		return patternBufPriority_;
	}

	int              getInputBufPriority()
	{
		return inputBufPriority_;
	}

	int              getOutputBufPriority()
	{
		return outputBufPriority_;
	}

	double           getUpdateTimeoutSecs()
	{
		return updateTimeout_;
	}

	BsaCorePtr create();
};

#endif
