#include <BsaPosixClock.h>
#include <stdexcept>
#include <stdio.h>
#include <errno.h>
#include <string.h>

BsaPosixTime::BsaPosixTime()
{
}

BsaPosixTime::BsaPosixTime(time_t sec, long nsec)
{
	tv_sec  = sec;
	tv_nsec = nsec;
}

BsaPosixTime::BsaPosixTime(uint64_t nsec)
{
	tv_sec  = nsec/1000000000ULL;
	tv_nsec = nsec%1000000000ULL;
}

BsaPosixTime
BsaPosixTime::operator+(const BsaPosixTime &rhs)
{
	BsaPosixTime res;
	res.tv_sec = tv_sec + rhs.tv_sec;
	if ( (res.tv_nsec = tv_nsec + rhs.tv_nsec) > 1000000000LL ) {
		res.tv_nsec -= 1000000000LL;
		res.tv_sec  += 1;
	}
	return res;
}

BsaPosixTime &
BsaPosixTime::operator+=(const BsaPosixTime &rhs)
{
	tv_sec += rhs.tv_sec;
	if ( (tv_nsec += rhs.tv_nsec) > 1000000000LL ) {
		tv_nsec -= 1000000000LL;
		tv_sec  += 1;
	}
	return *this;
}

namespace BsaAlias {
	BsaPosixTime
	nanoseconds(uint64_t ns)
	{
		return BsaPosixTime( ns );
	}
};

clockid_t
BsaPosixClock::getclock()
{
		return CLOCK_MONOTONIC;
}

BsaPosixTime
BsaPosixClock::now()
{
	BsaPosixTime rval;
	if ( clock_gettime( getclock(), &rval ) ) {
		throw std::runtime_error(std::string("clock_gettime failed: ") + std::string(strerror(errno)));
	}
	return rval;
}
