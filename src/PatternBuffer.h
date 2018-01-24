#ifndef BSA_PATTERN_BUFFER_H
#define BSA_PATTERN_BUFFER_H

#include <bsaCallbackApi.h>
#include <BsaApi.h>
#include <BsaTimeStamp.h>
#include <mutex>
#include <memory>
#include <atomic>
#include <RingBufferSync.h>

typedef uint16_t PatternIdx;

#define NUM_EDEF_MAX 64

typedef struct BsaPattern : public BsaTimingData {
private:
	std::atomic<int>      refCount_;
	PatternIdx            seqIdx_[NUM_EDEF_MAX];

public:
	BsaPattern(const BsaTimingData *p)
	: BsaTimingData( *p    ),
	  refCount_    ( 0     )
	{
	}

	BsaPattern()
	: refCount_( 0     )
	{
	}

	BsaPattern(const BsaPattern &orig)
	: BsaTimingData( orig         ),
	  refCount_    ( 0            )
	{
		for ( unsigned i = 0; i < NUM_EDEF_MAX; i++ )
			seqIdx_[i] = orig.seqIdx_[i];
	}

	BsaPattern &operator=(const BsaTimingData*);

	void incRef()
	{
		refCount_++;
	}
	int  decRef()
	{
		return --refCount_;
	}

	int  getRef() const
	{
		return refCount_.load();
	}

	friend class PatternBuffer;
} BsaPattern;

class PatternExpired  {};
class PatternTooNew   {};
class PatternNotFound {};

class PatternBuffer;

class FinalizePopCallback {
public:
	virtual void finalizePop(PatternBuffer *) = 0;

	virtual ~FinalizePopCallback(){}
};

class PatternBuffer : public RingBufferSync<BsaPattern, const BsaTimingData*> {
private:
	PatternBuffer(const PatternBuffer &);
	PatternBuffer & operator=(const PatternBuffer&);

    typedef std::unique_ptr< RingBuffer<PatternIdx> > IndexBufPtr;

	std::vector<IndexBufPtr>           indexBufs_;
	std::vector<FinalizePopCallback*>  finalizeCallbacks_;              

protected:
	virtual bool checkMinFilled();

	virtual void finalizePush();
	virtual void finalizePop();

public:

	PatternBuffer(unsigned ldSz, unsigned minfill);

	virtual BsaPattern *patternGet(BsaTimeStamp ts);

	virtual BsaPattern *patternGet(BsaPattern *);

	virtual BsaPattern *patternGetOldest(BsaEdef edef);

	virtual BsaPattern *patternGetNext(BsaPattern *pat, BsaEdef edef);

	virtual void addFinalizePop(FinalizePopCallback *);

	virtual void patternPut(BsaPattern *pattern);

	virtual void push_back(const BsaTimingData *pat);

};

#endif
