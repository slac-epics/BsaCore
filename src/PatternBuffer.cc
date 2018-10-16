#include <PatternBuffer.h>
#include <BsaDebug.h>
#include <stdexcept>

#undef  PBUF_PARANOIA

#define DBG(msg...) BSA_CORE_DBG(BSA_CORE_DEBUG_PATTERN,msg)
typedef unsigned long long dull;

#undef  ALWAYS_AGE_PATTERNS

BsaPattern &
BsaPattern::operator=(const BsaTimingData *p)
{
	if ( refCount_.load() != 0 ) {
		fprintf(stderr,"REFCNT: %d\n", refCount_.load());
		throw std::runtime_error("BsaPattern::operator=(const BsaTimingData *) -- refcount was not zero");
	}
	*(BsaTimingData*)this     = *p;
	refCount_                 = 0;
	return *this;
}

BsaPattern &
BsaPattern::operator=(const BsaPattern &rhs)
{
	if ( refCount_.load() != 0 ) {
		fprintf(stderr,"REFCNT: %d\n", refCount_.load());
		throw std::runtime_error("BsaPattern::operator=(const BsaPattern&) -- refcount was not zero");
	}
	*(BsaTimingData*)this     = rhs;
	refCount_                 = 0;
	return *this;
}

PatternBuffer::PatternBuffer(unsigned ldSz, unsigned minfill)
: RingBufferSync<BsaPattern, const BsaTimingData*>( ldSz, 0, minfill ),
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

	DBG("finalizePop -- enter\n");

	while ( ! frontUnused() ) {
		DBG("finalizePop -- waiting\n");
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

	DBG("seqIdx: %u, indexBuf head %u\n", pat->seqIdx_[edef], indexBufs_[edef]->head());

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
	DBG("indexBufs[%d], %d, pattern buf head %d\n", idx, (*indexBufs_[edef])[idx], head());
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
unsigned siz;
	{
	Lock lg( getMtx() );
		pid = back().pulseId;
		siz = size();
	}
	if ( pid + 1 != pat->pulseId ) {
		fprintf(stderr,"Non-sequential pulse IDs: had %d, new %d (current size %d)\n", pid, pat->pulseId, siz);
	}
#endif

#ifdef BSA_CORE_DEBUG
	{
	Lock lg( getMtx() );
		DBG("      pushing pattern (pid %llu, front %llu)\n", (dull)pat->pulseId, (dull)front().pulseId);
	}
#endif

	if ( ! RingBufferSync<BsaPattern, const BsaTimingData*>::push_back( pat , false ) ) {
#ifdef BSA_CORE_DEBUG
		Lock lg( getMtx() );
		DBG("Buffer overrun\n");
		DBG("Front PID    %llu\n", (dull)front().pulseId);
		DBG("Front refcnt %u\n",   front().getRef());
#endif
		throw std::runtime_error("Pattern buffer overrun");
	}
}

void
BsaPattern::dump(FILE *f, int indent, int idx) const
{
int      i;
uint64_t anyMask = edefInitMask | edefActiveMask;
uint64_t m;

	fprintf(f,"%*s(%4d)PID:%16llu, TS:%9lu/%9lu, ref %d, this %p\n",
		indent,"",
        idx,
		(unsigned long long)pulseId,
		(unsigned long)timeStamp.secPastEpoch,
		(unsigned long)timeStamp.nsec,
		getRef(),
		this );
	indent += 6;
	fprintf(f,"%*si%16llx a%16llx d%16llx\n",
		indent,"",
		(unsigned long long)edefInitMask,
		(unsigned long long)edefActiveMask,
		(unsigned long long)edefAvgDoneMask);
	for ( i=0, m=1ULL; i<NUM_EDEF_MAX; i++, m<<=1 ) {
		if ( (i % 16) == 0 ) {
			fprintf(f,"\n%*s",indent,"");
		}
		if ( (m & anyMask) ) {
			fprintf(f,"[%3d]",seqIdx_[i]);
		} else {
			fprintf(f,"[xxx]");
		}
	}
	fputc('\n',f);
}


void
PatternBuffer::dump(FILE *f)
{
Lock lg( getMtx() );

unsigned long i, j, s;
int           indent = 2;

std::vector<IndexBufPtr>::const_iterator it;

	s = (unsigned long)size();
	fprintf(f,"Head: %d, Tail: %d, Size %ld\n", head(), tail(), s);
	fprintf(f,"Patterns:\n");
	for ( i=0; i<s; i++ ) {
		this->operator[](i).dump(f, indent, i);
	}
	fprintf(f,"IndexBufs:\n");
	for ( it = indexBufs_.begin(), i=0; it != indexBufs_.end(); ++it, ++i ) {
		s = (unsigned long)(*it)->size();
		fprintf(f,"%*s%ld: H: %ld, T: %ld, S: %ld",
			indent, "", i,
			(unsigned long)(*it)->head(),
			(unsigned long)(*it)->tail(),
			s);
		for ( j=0; j<s; j++ ) {
			if ( (j % 16) == 0 ) {
				fprintf(f,"\n%*s",indent+2,"");
			}
			fprintf(f, "%5d", (*it)->operator[](j));
		}
		fputc('\n',f);
	}
}
