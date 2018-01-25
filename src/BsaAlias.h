#ifndef BSA_NAMESPACE_ALIAS_H
#define BSA_NAMESPACE_ALIAS_H

#if __cplusplus > 199711L && 0
#include <atomic>
#include <memory>
#else
#include <boost/atomic/atomic.hpp>
#include <boost/move/unique_ptr.hpp>
#endif


namespace BsaAlias {
#if __cplusplus > 199711L && 0
	using namespace std;
#else
	using namespace boost;
	using namespace boost::movelib;
#endif
};

#endif
