#ifndef BSA_NAMESPACE_ALIAS_H
#define BSA_NAMESPACE_ALIAS_H

#if __cplusplus > 199711L
#include <atomic>
#include <memory>
#include <condition_variable>
#include <mutex>
#include <chrono>
#include <thread>
#else
#include <boost/atomic/atomic.hpp>
#include <boost/move/unique_ptr.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>
#include <boost/make_shared.hpp>
#include <BsaMutex.h>
#include <BsaCondVar.h>
#include <BsaPosixClock.h>
#endif


namespace BsaAlias {

#if __cplusplus > 199711L
	using namespace            std;
	typedef unique_lock<std::mutex> Guard;
	typedef std::chrono::time_point<std::chrono::steady_clock> Time;
	typedef std::chrono::steady_clock Clock;
	using   std::chrono::nanoseconds;
	using   std::this_thread::sleep_until;
#else
	using namespace            boost;
	using namespace            boost::movelib;
	typedef BsaMutex           mutex;
	typedef BsaCondVar         condition_variable;
	typedef BsaMutex::Guard    Guard;

	namespace cv_status {
		const BsaCondVar::cv_status timeout = BsaCondVar::timeout;
	};

	typedef BsaPosixTime       Time;
	typedef BsaPosixClock      Clock;

	Time                       nanoseconds(uint64_t);

#endif
};

#endif
