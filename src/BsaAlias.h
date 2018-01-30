#ifndef BSA_NAMESPACE_ALIAS_H
#define BSA_NAMESPACE_ALIAS_H

#if __cplusplus > 199711L
#include <atomic>
#include <memory>
#else
#include <boost/atomic/atomic.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>
#include <boost/make_shared.hpp>
#endif

#if __cplusplus > 199711L && ! USE_PTHREADS
#include <condition_variable>
#include <mutex>
#include <chrono>
#else
#include <BsaMutex.h>
#include <BsaCondVar.h>
#include <BsaPosixClock.h>
#endif


namespace BsaAlias {

#if __cplusplus > 199711L
	using namespace            std;
#else
	using namespace            boost;
	using namespace            boost::movelib;
#endif
#if __cplusplus > 199711L && ! USE_PTHREADS
	typedef unique_lock<std::mutex>                            Guard;
	typedef std::chrono::time_point<std::chrono::steady_clock> Time;
	typedef std::chrono::nanoseconds                           Period;
	using   std::chrono::nanoseconds;
	typedef std::chrono::steady_clock                          Clock;
#else
	typedef BsaMutex           mutex;
	typedef BsaCondVar         condition_variable;
	typedef BsaMutex::Guard    Guard;

	namespace cv_status {
		const BsaCondVar::cv_status timeout = BsaCondVar::timeout;
	};

	typedef BsaPosixTime       Time;
	typedef BsaPosixTime       Period;
	typedef BsaPosixClock      Clock;

	Time                       nanoseconds(uint64_t);
#endif
};

#endif
