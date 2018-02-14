#include <BsaCoreFactory.h>

BsaCoreFactory::BsaCoreFactory()
: ldBufSz_( DEFAULT_BSA_LD_PATTERNBUF_SZ            ),
  minfill_( 0                                       )
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

BsaCorePtr
BsaCoreFactory::create()
{
unsigned minfill = minfill_ ? minfill_ : ( 1<< ( ldBufSz_ - 1 ) );
	return BsaAlias::make_shared< BsaCore >( ldBufSz_, minfill );
}
