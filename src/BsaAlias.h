#ifndef BSA_NAMESPACE_ALIAS_H
#define BSA_NAMESPACE_ALIAS_H

#if __cplusplus > 199711L
#include <atomic>
#include <memory>
#include <condition_variable>
#include <mutex>
#else
#include <boost/atomic/atomic.hpp>
#include <boost/move/unique_ptr.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>
#include <boost/make_shared.hpp>
#include <BsaMutex.h>
#include <BsaCondVar.h>
#endif


namespace BsaAlias {

#if __cplusplus > 199711L
	using namespace            std;
	typedef unique_lock<std::mutex> Guard;
#else
	using namespace            boost;
	using namespace            boost::movelib;
	typedef BsaMutex           mutex;
	typedef BsaCondVar         condition_variable;
	typedef BsaMutex::Guard    Guard;
#endif
};

#endif
