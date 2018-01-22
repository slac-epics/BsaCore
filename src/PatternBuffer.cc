#include <PatternBuffer.h>
#include <stdexcept>

BsaPattern &
BsaPattern::operator=(const BsaTimingData *p)
{
	refCount_ = 0;
	*(BsaTimingData*)this     = *p;
	return *this;
}

PatternBuffer::PatternBuffer(unsigned ldSz, unsigned minfill)
: RingBufferSync<BsaPattern, const BsaTimingData*>( ldSz, minfill )
{
unsigned i;
	for ( i=0; i<NUM_EDEF_MAX; i++ ) {
		indexBufs_.push_back( IndexBufPtr( new RingBuffer<PatternIdx>( ldSz ) ) );
	}
}

bool
PatternBuffer::checkMinFilled()
{
	return RingBufferSync<BsaPattern, const BsaTimingData*>::checkMinFilled() && front().getRef() == 0;
}

BsaPattern *
PatternBuffer::patternGet(BsaTimeStamp ts)
{
Lock lg( getMtx() );
int  inew = -1;
int  iold = -size();

	while ( iold <= inew ) {
		int         im = (inew + iold) / 2;

		if ( ts < (*this)[im].timeStamp ) {
			inew = im - 1;
		} else if ( ts > (*this)[im].timeStamp ) {
			iold = im + 1;
		} else {
		/* found it */
		BsaPattern *rval = &(*this)[im];
			rval->incRef();
			return rval;
		}
	}
	if ( iold == 0 )
		throw PatternTooNew();
	if ( inew + size() < 0 )
		throw PatternExpired();
	throw PatternNotFound();
}

BsaPattern *
PatternBuffer::patternGet(BsaPattern *p)
{
// assume caller already holds a reference, so no locking is necessary
// (refcount is atomic)
	if ( p->getRef() <= 0 ) {
		throw std::runtime_error("invalid pattern reference count");
	}
	p->incRef();
	return p;
}

void
PatternBuffer::patternPut(BsaPattern *p)
{
Lock lg( getMtx() );
	if ( p == &front() && checkMinFilled() ) {
		lg.unlock();
		notifyMinFilled();
	}
}

void
PatternBuffer::finalizePush()
{
BsaPattern &pat( back() );
unsigned    i;
uint64_t    anyMask = pat.edefInitMask | pat.edefActiveMask;
uint64_t    m;
PatternIdx  h = head();

	for ( i=0, m=1; anyMask; i++, m<<=1 ) {
		if ( (anyMask & m) ) {
			indexBufs_[i]->push_back( h );
			pat.seqIdx_[i] = indexBufs_[i]->head();
			anyMask &= ~m;
		}
	}
	
}

void
PatternBuffer::finalizePop()
{
BsaPattern &pat( back() );
unsigned    i;
uint64_t    anyMask = pat.edefInitMask | pat.edefActiveMask;
uint64_t    m;

std::vector<FinalizePopCallback*>::iterator it;

	for ( it = finalizeCallbacks_.begin(); it != finalizeCallbacks_.end(); ++it ) {
		(*it)->finalizePop( this );
	}

	for ( i=0, m=1; anyMask; i++, m<<=1 ) {
		if ( (anyMask & m) ) {
			indexBufs_[i]->pop();
			anyMask &= ~m;
		}
	}
}

void
PatternBuffer::addFinalizePop(FinalizePopCallback *cb)
{
	finalizeCallbacks_.push_back( cb );
}

BsaPattern *
PatternBuffer::patternGetNext(BsaPattern *pat, BsaEdef edef)
{
Lock lg( getMtx() );

	if ( ! pat || ! ( (pat->edefInitMask | pat->edefActiveMask) & (1<<edef)) )
		return 0;
	unsigned idx = pat->seqIdx_[edef] - indexBufs_[edef]->head() + 1;
	if ( idx >= indexBufs_[edef]->size() ) {
		// no newer active pattern available
		return 0;
	}
	BsaPattern *rval = & (*this)[ (*indexBufs_[edef])[idx] ];
	rval->incRef();
	return rval;
}

void
PatternBuffer::push_back(const BsaTimingData *pat)
{
uint64_t anyMask = pat->edefInitMask | pat->edefActiveMask;

	/* don't bother storing a pattern that is not involved in BSA */
	if ( !  anyMask ) {
		return;
	}

	if ( ! RingBufferSync<BsaPattern, const BsaTimingData*>::push_back( pat , false ) ) {
		throw std::runtime_error("Pattern buffer overrun");
	}
}
