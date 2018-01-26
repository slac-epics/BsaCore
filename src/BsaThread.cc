#include <unistd.h>
#include <BsaThread.h>
#include <pthread.h>
#include <string.h>
#include <stdio.h>
#include <signal.h>
#include <stdexcept>
#include <errno.h>

#define BSA_THREAD_DEBUG


class BsaThreadWrapper {
private:
	pthread_t tid_;

	static void *thread_fun(void *arg);

	BsaThreadWrapper(const BsaThreadWrapper&);
	BsaThreadWrapper &operator=(const BsaThreadWrapper&);

public:
	BsaThreadWrapper(BsaThread *runner);

	// RAII for pthread_attr_t
	class Attr {
	private:
		pthread_attr_t a_;
		Attr(const Attr &);
		Attr &operator=(const Attr &);

	public:
		Attr();
		pthread_attr_t *getp()
		{
			return &a_;
		}
		~Attr();
	};

	pthread_t native_handle()
	{
		return tid_;
	}

	// RAII for signal mask
	class SigMask {
	private:
		sigset_t orig_;
		SigMask(const SigMask &);
		SigMask &operator=(const SigMask &);
	public:
		SigMask();
		~SigMask();
	};
};

void * BsaThreadWrapper::thread_fun(void *arg)
{
BsaThread *me = (BsaThread*)arg;
	me->run();
	return 0;
}

BsaThread::BsaThread(const char *nam)
: nam_( nam              ),
  pri_( DEFAULT_PRIORITY )
{
}

BsaThread::~BsaThread()
{
#ifdef BSA_THREAD_DEBUG
	printf("Destroying %s\n", getName());
#endif
	stop();

}

void
BsaThread::start()
{
#ifdef BSA_THREAD_DEBUG
	printf("Starting %s\n", getName());
#endif
	tid_ = Thread( new BsaThreadWrapper( this ) );
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
int st;
	pthread_t tid = tid_->native_handle();
	if ( (st = pthread_join( tid, NULL )) ) {
		printf("%s\n", strerror(st));
		throw std::runtime_error("pthread_join failed");
	}
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

int
BsaThread::getPriority()
{
	return pri_;
}

static int
checkErr(BsaThread *t, const char *name, int err, int priority)
{
	if ( ! err )
		return 0;
	if ( EPERM == err && priority > 0 ) {
		fprintf(stderr,"WARNING: unable to set priority for %s -- (%s): IGNORED\n", t->getName(), ::strerror(err));
	} else {
		throw std::runtime_error( std::string(name) + std::string(" failed: ") + std::string(::strerror(err)) );
	}
	return err;
}

int
BsaThread::setPriority(int pri)
{
	if ( tid_ ) {
	#if defined _POSIX_THREAD_PRIORITY_SCHEDULING
        int                pol;
        struct sched_param param;

		pol                  = pri > 0 ? SCHED_FIFO : SCHED_OTHER;
		param.sched_priority = pri > 0 ? pri : 0;

		int                err;

		if ( (err = pthread_setschedparam( tid_->native_handle(), pol, &param ) ) ) {
			// throws if not recoverable
			checkErr(this, "pthread_setschedparam", err, pri);
			return getPriority();
		}

	#else
		fprintf(stderr,"BsaThread (setPriority) -- unable to use real-time scheduler; no OS support at compile time (IGNORED)\n");
		return getPriority();
	#endif
	}
	pri_ = pri;
	return getPriority();
}

const char *
BsaThread::getName()
{
	return nam_.c_str();
}


BsaThreadWrapper::Attr::Attr()
{
int err;
	if ( (err = pthread_attr_init( &a_ )) ) {
		throw std::runtime_error( std::string("pthread_attr_init failed: ") + std::string(::strerror(err)) );
	}
}

BsaThreadWrapper::Attr::~Attr()
{
	pthread_attr_destroy( &a_ );
}

BsaThreadWrapper::SigMask::SigMask()
{
int err;
sigset_t all;
	sigfillset( &all );
	if ( (err = pthread_sigmask( SIG_SETMASK, &all, &orig_ )) ) {
		throw std::runtime_error( std::string("pthread_sigmask (block all) failed: ") + std::string(::strerror(err)) );
	}
}

BsaThreadWrapper::SigMask::~SigMask()
{
int err;
	if ( (err = pthread_sigmask( SIG_SETMASK, &orig_, NULL )) ) {
		throw std::runtime_error( std::string("pthread_sigmask (restore) failed: ") + std::string(::strerror(err)) );
	}
}

BsaThreadWrapper::BsaThreadWrapper(BsaThread *runner)
{
int     err      = -1;
int     priority = runner->getPriority();
int     attempts;

    for ( attempts = 2; attempts > 0 && err; attempts-- ) {
        Attr    attr;
		SigMask blockSignals; // block signals so that the new thread inherits an all-blocking mask

#if defined _POSIX_THREAD_PRIORITY_SCHEDULING
        int                pol;
        struct sched_param param;

		pol                  = priority > 0 ? SCHED_FIFO : SCHED_OTHER;
		param.sched_priority = priority > 0 ? priority : 0;

        if ( (err = pthread_attr_setschedpolicy( attr.getp(), pol )) ) {
			throw std::runtime_error( std::string("pthread_attr_setschedpolicy failed: ") + std::string(::strerror(err)) );
        }
        if ( SCHED_OTHER != pol ) {
            if ( (err = pthread_attr_setschedparam( attr.getp(), &param )) ) {
				throw std::runtime_error( std::string("pthread_attr_setschedparam(prio) failed: ") + std::string(::strerror(err)) );
            }
        }
        if ( (err = pthread_attr_setinheritsched( attr.getp(), PTHREAD_EXPLICIT_SCHED)) ) { 
			throw std::runtime_error( std::string("pthread_attr_setinheritsched failed: ") + std::string(::strerror(err)) );
        }
#else
        #warning "_POSIX_THREAD_PRIORITY_SCHEDULING not defined -- always using default priority"
		fprintf(stderr,"BsaThread -- unable to use real-time scheduler; no OS support at compile time\n");
        priority = 0;
#endif
        if ( (err = pthread_create( &tid_, attr.getp(), thread_fun, (void*)runner )) ) {
			// throws if not recoverable
			checkErr(runner, "pthread_create", err, priority);
		}
    }
	// communicate possibly corrected value back
	runner->setPriority( priority );
}
