#include <PatternBuffer.h>
#include <stdexcept>

PatternBuffer::PatternBuffer(unsigned ldSz)
: RingBuffer<BsaPattern>( ldSz )
{
}

BsaPattern *
PatternBuffer::patternGet(BsaTimeStamp ts)
{
Lock lg( mtx_ );
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

void
PatternBuffer::patternPut(BsaPattern *p)
{
Lock lg( mtx_ );
	p->decRef();
}

void
PatternBuffer::push_back(const BsaTimingData *pat)
{
Lock lg( mtx_ );
	if ( full() ) {
		// retire oldest pattern
		if ( front().getRef() > 0 ) {
			throw std::runtime_error("Pattern buffer overrun (oldest pattern still in use; can't retire)");
		}
		RingBuffer<BsaPattern>::pop();
	}
	RingBuffer<BsaPattern>::push_back( BsaPattern( pat ) );
}
