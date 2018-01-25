#include <BsaThread.h>
#include <string.h>
#include <stdio.h>
#include <thread>


void BsaThread::thread_fun(BsaThread *me)
{
	me->run();
}

BsaThread::BsaThread(const char *nam)
: nam_( nam )
{
}

BsaThread::~BsaThread()
{
#ifdef BSA_THREAD_DEBUG
	printf("Destroying %s\n", nam_.c_str());
#endif
	stop();
}

void
BsaThread::start()
{
#ifdef BSA_THREAD_DEBUG
	printf("Starting %s\n", nam_.c_str());
#endif
	tid_ = Thread( new std::thread( thread_fun, this ) );
}

void
BsaThread::kill()
{
int st;
	pthread_t tid = tid_->native_handle();
	if ( (st = pthread_cancel( tid )) ) {
		printf("%s\n", strerror(st));
		throw std::runtime_error("pthread_cancel failed");
	}
}

void
BsaThread::join()
{
	tid_->join();
	tid_.reset();
}


void
BsaThread::stop()
{
	if ( tid_ ) {
		kill();
		join();
	}
}
