#include <PatternBuffer.h>
#include <stdexcept>

#undef  PBUF_PARANOIA

#undef  PBUF_DEBUG

#undef  ALWAYS_AGE_PATTERNS

BsaPattern &
BsaPattern::operator=(const BsaTimingData *p)
{
	refCount_ = 0;
	*(BsaTimingData*)this     = *p;
	return *this;
}

PatternBuffer::PatternBuffer(unsigned ldSz, unsigned minfill)
: RingBufferSync<BsaPattern, const BsaTimingData*>( ldSz, minfill ),
  activeEdefSet_( 0 )
{
unsigned i;
	indexBufs_.reserve( NUM_EDEF_MAX );
	for ( i=0; i<NUM_EDEF_MAX; i++ ) {
		indexBufs_.push_back( IndexBufPtr( new RingBuffer<PatternIdx>( ldSz ) ) ) ;
	}
}

bool
PatternBuffer::frontUnused()
{
	return front().getRef() == 0;
}

BsaPattern *
PatternBuffer::patternGet(BsaTimeStamp ts)
{
Lock lg( getMtx() );
int  inew = size() - 1;
int  iold = 0;

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
	if ( iold == (int)size() )
		throw PatternTooNew();
	if ( inew < 0 )
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
PatternBuffer::notifyFrontUnused()
{
	frontUnused_.notify_one();
}

void
PatternBuffer::patternPut(BsaPattern *p)
{
bool doNotify;
	{
	Lock lg( getMtx() );
		doNotify = ( 0 == p->decRef() && p == &front() );
	}
	if ( doNotify ) {
		notifyFrontUnused();
	}
}

void
PatternBuffer::finalizePush(Lock *lp)
{
BsaPattern &pat( back() );
unsigned    i;
uint64_t    anyMask = pat.edefInitMask | pat.edefActiveMask;
uint64_t    m;
PatternIdx  t = tail();

	for ( i=0, m=1; anyMask; i++, m<<=1 ) {
		if ( (anyMask & m) ) {
			activeEdefSet_ |= m;
			indexBufs_[i]->push_back( t );
			pat.seqIdx_[i] = indexBufs_[i]->tail();
			anyMask &= ~m;
		}
	}
	
}

void
PatternBuffer::finalizePop(Lock *lp)
{
BsaPattern &pat( front() );
unsigned    i;
uint64_t    anyMask = pat.edefInitMask | pat.edefActiveMask;
uint64_t    m;

#ifdef PBUF_DEBUG
printf("finalizePop -- enter\n");
#endif

	while ( ! frontUnused() ) {
#ifdef PBUF_DEBUG
printf("finalizePop -- waiting\n");
#endif
		frontUnused_.wait( *lp ); 
	}

std::vector<FinalizePopCallback*>::iterator it;

	for ( it = finalizeCallbacks_.begin(); it != finalizeCallbacks_.end(); ++it ) {
		(*it)->finalizePop( this );
	}

	for ( i=0, m=1; anyMask; i++, m<<=1 ) {
		if ( (anyMask & m) ) {
			indexBufs_[i]->pop();
			if ( 0 == indexBufs_[i]->size() ) {
				activeEdefSet_ &= ~m;
			}
			anyMask &= ~m;
		}
	}
}

void
PatternBuffer::addFinalizePop(FinalizePopCallback *cb)
{
Lock lg( getMtx() );
	finalizeCallbacks_.push_back( cb );
}

BsaPattern *
PatternBuffer::patternGetNext(BsaPattern *pat, BsaEdef edef)
{
Lock lg( getMtx() );

	if ( ! pat || ! ( (pat->edefInitMask | pat->edefActiveMask) & (1<<edef)) )
		return 0;

#if defined( PBUF_DEBUG )
printf("seqIdx: %u, indexBuf head %u\n", pat->seqIdx_[edef], indexBufs_[edef]->head());
#endif

	unsigned idx = ( (pat->seqIdx_[edef] - indexBufs_[edef]->head()) & mask() );

#ifdef PBUF_PARANOIA
	if ( (BsaTimeStamp)pat->timeStamp != (BsaTimeStamp)(*this)[ (*indexBufs_[edef])[idx] - head() ].timeStamp ) {
		throw std::runtime_error("Internal Error: pattern mismatch");
	}
#endif

	idx = idx + 1;

	if ( idx >= indexBufs_[edef]->size() ) {
		// no newer active pattern available
		return 0;
	}
#if defined( PBUF_DEBUG )
printf("indexBufs[%d], %d, pattern buf head %d\n", idx, (*indexBufs_[edef])[idx], head());
#endif
	BsaPattern *rval = & (*this)[ (*indexBufs_[edef])[idx] - head() ];
	rval->incRef();
	return rval;
}

BsaPattern *
PatternBuffer::patternGetOldest(BsaEdef edef)
{
Lock lg( getMtx() );

	if ( indexBufs_[edef]->size() < 1 )
		return 0;
	BsaPattern * rval = & (*this)[ indexBufs_[edef]->front() - head() ];
	rval->incRef();
	return rval;
}

void
PatternBuffer::push_back(const BsaTimingData *pat)
{
#ifndef ALWAYS_AGE_PATTERNS
uint64_t anyMask = pat->edefInitMask | pat->edefActiveMask;

	/* don't bother storing a pattern that is not involved in BSA
	 * HOWEVER: this relies on some continuous BSA going on -- otherwise
	 *          patterns may never 'age' out.
	 */
	if ( !  anyMask ) {
		return;
	}
#else
uint64_t pid;
	{
	Lock lg( getMtx() );
		pid = back().pulseId;
	}
	if ( pid + 1 != pat->pulseId ) {
		fprintf(stderr,"Non-sequential pulse IDs: had %d, new %d\n", pid, pat->pulseId);
	}
#endif

#ifdef PBUF_DEBUG
	{
	Lock lg( getMtx() );
		printf("      pushing pattern (pid %llu, front %llu)\n", (unsigned long long)pat->pulseId, (unsigned long long)front().pulseId);
	}
#endif

	if ( ! RingBufferSync<BsaPattern, const BsaTimingData*>::push_back( pat , false ) ) {
#ifdef PBUF_DEBUG
		Lock lg( getMtx() );
		printf("Buffer overrun\n");
		printf("Front PID    %llu\n", (unsigned long long)front().pulseId);
		printf("Front refcnt %u\n",   front().getRef());
#endif
		throw std::runtime_error("Pattern buffer overrun");
	}
}
