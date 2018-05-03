#ifndef BSA_POSIX_CLOCK_WRAPPER_H
#define BSA_POSIX_CLOCK_WRAPPER_H

// BsaTimeStamp is representing EPICS time. However, for
// the different pthread (or c++11) timing features we
// need a different clock and epoch...

#include <time.h>
#include <stdint.h>

#ifdef __rtems__
#	include <rtems.h>
#	if !defined(__RTEMS_MAJOR__)
#		error "Unable to determine RTEMS version"
#	endif
#	if (__RTEMS_MAJOR__ < 4) || (__RTEMS_MAJOR__ == 4 && __RTEMS_MINOR__ <= 11)
#	   define HAS_NO_CONDATTR_SETCLOCK
#	endif
#endif

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
