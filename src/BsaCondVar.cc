#include <BsaCondVar.h>
#include <BsaMutex.h>
#include <BsaPosixClock.h>
#include <stdexcept>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <stdio.h>

BsaCondVar::Attr::Attr()
{
int err;
	if ( (err = pthread_condattr_init( &a_ )) ) {
		throw std::runtime_error( std::string("pthread_condattr_init failed: ") + std::string(::strerror(err)) );
	}
#if ! defined(HAS_NO_CONDATTR_SETCLOCK)
	if ( (err = pthread_condattr_setclock( &a_, BsaPosixClock::getclock() )) ) {
		throw std::runtime_error( std::string("pthread_condattr_setclock failed: ") + std::string(::strerror(err)) );
	}
#endif
}

BsaCondVar::Attr::~Attr()
{
	pthread_condattr_destroy( &a_ );
}

pthread_condattr_t *
BsaCondVar::Attr::get()
{
	return &a_;
}

BsaCondVar::BsaCondVar()
{
int err;
Attr att;

	if ( (err = pthread_cond_init( &c_, att.get() )) ) {
		throw std::runtime_error( std::string("pthread_cond_init failed: ") + std::string(::strerror(err)) );
	}
}

BsaCondVar::~BsaCondVar()
{
	pthread_cond_destroy( &c_ );
}

void
BsaCondVar::wait(BsaMutex::Guard &mtx)
{
int err;

pthread_mutex_t *m = mtx.get();


	err = pthread_cond_wait( &c_, m );

	if ( err ) {
		throw std::runtime_error( std::string("pthread_cond_wait failed: ") + std::string(::strerror(err)) );
	}
}

BsaCondVar::cv_status
BsaCondVar::wait_until(BsaMutex::Guard &mtx, const BsaPosixTime &abs_timeout)
{
int err;

pthread_mutex_t *m = mtx.get();

	err = pthread_cond_timedwait( &c_, m, &abs_timeout );

	if ( 0 == err ) {
		return no_timeout;
	}

	if ( ETIMEDOUT == err ) {
		return timeout;
	}

	throw std::runtime_error( std::string("pthread_cond_timedwait failed: ") + std::string(::strerror(err)) );
}

void
BsaCondVar::notify_one()
{
int err;
	if ( (err = pthread_cond_signal( &c_ )) ) {
		throw std::runtime_error( std::string("pthread_cond_signal failed: ") + std::string(::strerror(err)) );
	}
}

