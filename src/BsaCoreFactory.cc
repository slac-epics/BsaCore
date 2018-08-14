#include <BsaCoreFactory.h>

BsaCoreFactory::BsaCoreFactory()
: ldBufSz_           ( DEFAULT_BSA_LD_PATTERNBUF_SZ    ),
  minfill_           (  0                              ),
  patternBufPriority_( BsaThread::getDefaultPriority() ),
  inputBufPriority_  ( BsaThread::getDefaultPriority() ),
  outputBufPriority_ ( BsaThread::getDefaultPriority() ),
  updateTimeout_     ( (double)DEFAULT_BSA_UPDATE_TIMEOUT_MS/1000.0 ),
  numInpBufs_        ( DEFAULT_NUM_INP_BUFS            ),
  numOutBufs_        ( DEFAULT_NUM_OUT_BUFS            )
{
	if ( inputBufPriority_ > BsaThread::getPriorityMin() ) {
		inputBufPriority_--;
		outputBufPriority_--;
	}
	if ( outputBufPriority_ > BsaThread::getPriorityMin() ) {
		outputBufPriority_--;
	}
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
BsaCoreFactory::setPatternBufPriority(int val)
{
	patternBufPriority_ = val;
	return *this;
}

BsaCoreFactory &
BsaCoreFactory::setInputBufPriority(int val)
{
	inputBufPriority_ = val;
	return *this;
}

BsaCoreFactory &
BsaCoreFactory::setOutputBufPriority(int val)
{
	outputBufPriority_ = val;
	return *this;
}

BsaCoreFactory &
BsaCoreFactory::setNumInpBufs(unsigned val)
{
	numInpBufs_ = val;
	return *this;
}

BsaCoreFactory &
BsaCoreFactory::setNumOutBufs(unsigned val)
{
	numOutBufs_ = val;
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
