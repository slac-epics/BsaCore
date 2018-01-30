#include <unistd.h>
#include <BsaMutex.h>
#include <stdexcept>
#include <stdio.h>
#include <string.h>

class BsaMutexAttr {
private:
	pthread_mutexattr_t a_;

	BsaMutexAttr(const BsaMutexAttr &);
	BsaMutexAttr & operator=(const BsaMutexAttr &);

public:
	BsaMutexAttr()
	{
	int err;
		if ( (err = pthread_mutexattr_init( &a_ )) ) {
			throw std::runtime_error( std::string("pthread_mutexattr_init failed: ") + std::string(::strerror(err)) );
		}
#if defined(_POSIX_THREAD_PRIO_INHERIT)
		if ( (err = pthread_mutexattr_setprotocol( &a_, PTHREAD_PRIO_INHERIT )) ) {
			throw std::runtime_error( std::string("pthread_mutexattr_setprotocol failed: ") + std::string(::strerror(err)) );
		}
#else
#warning("_POSIX_THREAD_PRIO_INHERIT undefined on this system -- no mutex priority inheritance available!");
		fprintf(stderr,"WARNING: _POSIX_THREAD_PRIO_INHERIT not supported\n");
#endif
	}

	~BsaMutexAttr()
	{
		pthread_mutexattr_destroy( &a_ );
	}

	const pthread_mutexattr_t *operator*()
	{
		return &a_;
	}

};

BsaMutex::BsaMutex()
{
int          err;
BsaMutexAttr attr;
	if ( (err = pthread_mutex_init( &mtx_, *attr )) ) {
			throw std::runtime_error( std::string("pthread_mutex_init failed: ") + std::string(::strerror(err)) );
	}
}

BsaMutex::~BsaMutex()
{
	pthread_mutex_destroy( &mtx_ );
}

void
BsaMutex::lock()
{
int err;
	if ( (err = pthread_mutex_lock( &mtx_ )) ) {
			throw std::runtime_error( std::string("pthread_mutex_lock failed: ") + std::string(::strerror(err)) );
	}
}

void
BsaMutex::unlock()
{
int err;
	if ( (err = pthread_mutex_unlock( &mtx_ )) ) {
			throw std::runtime_error( std::string("pthread_mutex_unlock failed: ") + std::string(::strerror(err)) );
	}
}

pthread_mutex_t *
BsaMutex::get()
{
	return &mtx_;
}
