#ifndef BSA_PATTERN_BUFFER_H
#define BSA_PATTERN_BUFFER_H

#include <bsaCallbackApi.h>
#include <BsaTimeStamp.h>
#include <mutex>
#include <RingBuffer.h>

typedef struct BsaPattern : public BsaTimingData {
private:
	int refCount_;
public:
	BsaPattern(const BsaTimingData *p)
	: BsaTimingData( *p ),
	  refCount_    (  0 )
	{
	}
	void incRef()
	{
		refCount_++;
	}
	int  decRef()
	{
		return --refCount_;
	}

	int  getRef()
	{
		return refCount_;
	}
} BsaPattern;

class PatternExpired  {};
class PatternTooNew   {};
class PatternNotFound {};

class PatternBuffer : public RingBuffer<BsaPattern> {
private:
	PatternBuffer(const PatternBuffer &);
	PatternBuffer & operator=(const PatternBuffer&);

	std::mutex mtx_;

	typedef std::unique_lock<std::mutex> Lock;

	bool pop();

public:
	PatternBuffer(unsigned ldSz);

	virtual BsaPattern *patternGet(BsaTimeStamp ts);

	virtual void patternPut(BsaPattern *pattern);

	virtual void push_back(const BsaTimingData *pat);

};

#endif
