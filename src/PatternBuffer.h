#ifndef BSA_PATTERN_BUFFER_H
#define BSA_PATTERN_BUFFER_H

#include <bsaCallbackApi.h>
#include <BsaApi.h>
#include <BsaTimeStamp.h>
#include <RingBufferSync.h>
#include <BsaAlias.h>
#include <stdio.h>

typedef uint16_t PatternIdx;

#define NUM_EDEF_MAX 64

typedef struct BsaPattern : public BsaTimingData {
private:
	typedef BsaAlias::atomic<int>   RefCount;
	RefCount              refCount_;
	PatternIdx            seqIdx_[NUM_EDEF_MAX];

public:
	BsaPattern(const BsaTimingData *p)
	: refCount_    ( 0     )
	{
		if ( p )
			*this = p;
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
	BsaPattern &operator=(const BsaPattern &);

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

	void dump(FILE *f, int indent, int idx) const;

	friend class PatternBuffer;
} BsaPattern;

class PatternExpired  : public std::runtime_error {
public:
	PatternExpired()  : runtime_error("PatternExpired")
	{}
};
class PatternTooNew   : public std::runtime_error {
public:
	PatternTooNew()   : runtime_error("PatternTooNew")
	{}
};
class PatternNotFound : public std::runtime_error {
public:
	PatternNotFound() : runtime_error("PatternNotFound")
	{}
};

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

	uint64_t                           activeEdefSet_;

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

	virtual void dump(FILE *);

};

#endif
