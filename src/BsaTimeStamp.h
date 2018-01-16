#ifndef BSA_TIME_STAMP_H
#define BSA_TIME_STAMP_H

#include <stdint.h>
#include <stdio.h>

typedef struct BsaTimeStamp {
	uint64_t ts_;
#ifdef __cplusplus
public:
	BsaTimeStamp()
	: ts_(0LL)
	{
	}
	BsaTimeStamp(const epicsTimeStamp &rhs)
	{
		ts_ = (((uint64_t)rhs.secPastEpoch) << 32) | ((uint64_t)rhs.nsec);
	}

	operator epicsTimeStamp()
	{
	epicsTimeStamp ts;
		ts.secPastEpoch = ts_ >> 32;
		ts.nsec         = ts_;
		return ts;
	}

	bool operator == (const BsaTimeStamp &rhs) const { return ts_ == rhs.ts_; }
	bool operator != (const BsaTimeStamp &rhs) const { return ts_ != rhs.ts_; }
	bool operator <= (const BsaTimeStamp &rhs) const { return ts_ <= rhs.ts_; }
	bool operator <  (const BsaTimeStamp &rhs) const { return ts_ <  rhs.ts_; }
	bool operator >= (const BsaTimeStamp &rhs) const { return ts_ >= rhs.ts_; }
	bool operator >  (const BsaTimeStamp &rhs) const { return ts_ >  rhs.ts_; }
#endif
} BsaTimeStamp;

#endif
