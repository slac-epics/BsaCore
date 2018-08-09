#include <BsaCoreFactory.h>

BsaCoreFactory::BsaCoreFactory()
: ldBufSz_           ( DEFAULT_BSA_LD_PATTERNBUF_SZ ),
  minfill_           (  0                           ),
  patternBufPriority_( -1                           ),
  inputBufPriority_  ( -1                           ),
  outputBufPriority_ ( -1                           ),
  updateTimeout_     ( (double)DEFAULT_BSA_UPDATE_TIMEOUT_MS/1000.0 )
{
}

BsaCoreFactory &
BsaCoreFactory::setLdBufSz(unsigned val)
{
	ldBufSz_ = val;
	return *this;
}

BsaCoreFactory &
BsaCoreFactory::setMinFill(unsigned val)
{
	minfill_ = val;
	return *this;
}

BsaCoreFactory &
BsaCoreFactory::setPatternBufPriority(unsigned val)
{
	patternBufPriority_ = val;
	return *this;
}

BsaCoreFactory &
BsaCoreFactory::setInputBufPriority(unsigned val)
{
	inputBufPriority_ = val;
	return *this;
}

BsaCoreFactory &
BsaCoreFactory::setOutputBufPriority(unsigned val)
{
	outputBufPriority_ = val;
	return *this;
}

BsaCoreFactory &
BsaCoreFactory::setUpdateTimeoutSecs(double val)
{
	updateTimeout_ = val;
	return *this;
}

BsaCorePtr
BsaCoreFactory::create()
{
	return BsaAlias::make_shared< BsaCore >( this );
}
