#ifndef BSA_TIME_STAMP_H
#define BSA_TIME_STAMP_H

#include <stdint.h>
#include <stdio.h>

typedef struct BsaTimeStamp {
	uint64_t ts_;
#ifdef __cplusplus
private:
	static const uint64_t SEC =  1000000000ULL;
	static const unsigned SHF = 32;
	static const uint64_t ONE = 1ULL << SHF;
	BsaTimeStamp(uint64_t raw, bool isFormatted)
	: ts_(raw)
	{
	}

public:
	BsaTimeStamp(uint64_t ns)
	: ts_( (ns % SEC) | ((ns/SEC)<<SHF) )
	{
	}

	BsaTimeStamp()
	: ts_(0ULL)
	{
	}

	BsaTimeStamp(const epicsTimeStamp &rhs)
	{
		ts_ = (((uint64_t)rhs.secPastEpoch) << SHF) | ((uint64_t)rhs.nsec);
	}

	operator epicsTimeStamp() const
	{
	epicsTimeStamp ts;
		ts.secPastEpoch = ts_ >> SHF;
		ts.nsec         = ts_;
		return ts;
	}

	operator uint64_t()
	{
		return (ts_ & (ONE-1)) + SEC*(ts_>>SHF);
	}

	bool operator == (const BsaTimeStamp &rhs) const { return ts_ == rhs.ts_; }
	bool operator != (const BsaTimeStamp &rhs) const { return ts_ != rhs.ts_; }
	bool operator <= (const BsaTimeStamp &rhs) const { return ts_ <= rhs.ts_; }
	bool operator <  (const BsaTimeStamp &rhs) const { return ts_ <  rhs.ts_; }
	bool operator >= (const BsaTimeStamp &rhs) const { return ts_ >= rhs.ts_; }
	bool operator >  (const BsaTimeStamp &rhs) const { return ts_ >  rhs.ts_; }
	bool operator !  (                       ) const { return     !      ts_; }

	BsaTimeStamp operator+(const BsaTimeStamp &rhs) const {
		uint64_t s = ts_ + (const uint64_t)rhs.ts_;
		if ( (s & (ONE-1)) >= SEC ) {
			s += ONE - SEC;
		}
		return BsaTimeStamp(s, true);
	}

	BsaTimeStamp operator+(uint32_t rhs) const {
		uint64_t rval = ts_;

		while ( rhs >= SEC ) {
			rval += ONE;
			rhs  -= SEC;
		}
		rval += rhs;
		if ( (rval & (ONE-1)) >= SEC ) {
			rval += ONE - SEC;
		}
		return BsaTimeStamp(rval, true);
	}

	static uint64_t nsDiff(const BsaTimeStamp &a, const BsaTimeStamp &b)
	{
	uint64_t d = a.ts_ - b.ts_;
		if ( (d & (ONE-1)) >= SEC ) {
			d += SEC - ONE;
		}
		return d;
	}
#endif
} BsaTimeStamp;

#endif
