#ifndef BSA_POSIX_CLOCK_WRAPPER_H
#define BSA_POSIX_CLOCK_WRAPPER_H

// BsaTimeStamp is representing EPICS time. However, for
// the different pthread (or c++11) timing features we
// need a different clock and epoch...

#include <time.h>
#include <stdint.h>

struct BsaPosixTime : public timespec {
public:
	BsaPosixTime();

	BsaPosixTime(time_t sec, long nsec);

	BsaPosixTime(uint64_t nsec);

	BsaPosixTime operator+(const BsaPosixTime &rhs);

	BsaPosixTime & operator+=(const BsaPosixTime &rhs);
};

class BsaPosixClock {
public:
	static clockid_t getclock();

	static BsaPosixTime now();
};

#endif
