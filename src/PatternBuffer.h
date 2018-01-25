#ifndef BSA_PATTERN_BUFFER_H
#define BSA_PATTERN_BUFFER_H

#include <bsaCallbackApi.h>
#include <BsaApi.h>
#include <BsaTimeStamp.h>
#include <RingBufferSync.h>
#include <BsaAlias.h>

typedef uint16_t PatternIdx;

#define NUM_EDEF_MAX 64

typedef struct BsaPattern : public BsaTimingData {
private:
	typedef BsaAlias::atomic<int>   RefCount;
	RefCount              refCount_;
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

    typedef RingBuffer<PatternIdx>           IndexBuf;
    typedef BsaAlias::shared_ptr< IndexBuf > IndexBufPtr;

	std::vector<IndexBufPtr>           indexBufs_;
	std::vector<FinalizePopCallback*>  finalizeCallbacks_;              

	CondVar                            frontUnused_;

protected:
	virtual bool frontUnused();

	virtual void finalizePush(Lock*);
	virtual void finalizePop(Lock*);
	virtual void notifyFrontUnused();

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
