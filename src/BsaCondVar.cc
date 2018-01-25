#include <BsaCondVar.h>
#include <BsaMutex.h>
#include <stdexcept>
#include <string.h>
#include <pthread.h>

BsaCondVar::BsaCondVar()
{
int err;
	if ( (err = pthread_cond_init( &c_, NULL )) ) {
		throw std::runtime_error( std::string("pthread_cond_init failed") + std::string(::strerror(err)) );
	}
}

BsaCondVar::~BsaCondVar()
{
	pthread_cond_destroy( &c_ );
}

struct XX {
	pthread_t locker;
	pthread_mutex_t *m;

	XX(pthread_mutex_t *m)
	: locker(pthread_self()),
	  m(m)
	{
	}
};

void
BsaCondVar::wait(BsaMutex::Guard &mtx)
{
int err;

pthread_mutex_t *m = mtx.get();

	err = pthread_cond_wait( &c_, m );

	if ( err ) {
		throw std::runtime_error( std::string("pthread_cond_wait failed") + std::string(::strerror(err)) );
	}
}

void
BsaCondVar::notify_one()
{
int err;
	if ( (err = pthread_cond_signal( &c_ )) ) {
		throw std::runtime_error( std::string("pthread_cond_signal failed") + std::string(::strerror(err)) );
	}
}

