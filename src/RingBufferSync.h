#ifndef BSA_RING_BUFFER_SYNC_H
#define BSA_RING_BUFFER_SYNC_H

#include <RingBuffer.h>
#include <BsaAlias.h>

// Ring buffer with synchronization (producer sleeps on full,
// consumer on empty).
template <typename T, typename ELT = const T&>
class RingBufferSync : public RingBuffer<T, ELT> {
protected:
	typedef BsaAlias::mutex                   Mutex;
	typedef BsaAlias::condition_variable      CondVar;
	typedef BsaAlias::Guard                   Lock;

private:
	unsigned                 minfill_;
	Mutex                    mtx_;
	CondVar                  minfilled_;
	CondVar                  not_full_;
	BsaAlias::Period         period_;
	BsaAlias::Time           timeout_;

protected:

	Mutex                    & getMtx()
	{
		return mtx_;
	}

	void notifyNotFull()
	{
		not_full_.notify_one();
	}	

	void notifyMinFilled()
	{
		minfilled_.notify_one();
	}

	virtual bool checkMinFilled()
	{
		return RingBuffer<T,ELT>::size() > minfill_ ;
	}

	virtual void finalizePush(Lock *lp)
	{
	}

	virtual void finalizePop(Lock *lp)
	{
	}

	bool wait(T *pv, bool doBlock, bool doPop)
	{
		{
		Lock l( mtx_ );
			if ( doBlock ) {
				while ( ! checkMinFilled() ) {
					minfilled_.wait( l );
				}
			} else {
				if ( ! checkMinFilled() ) {
					return false;
				}
			}
			if ( pv ) {
				*pv = RingBuffer<T,ELT>::front();
			}
			if ( doPop ) {
				finalizePop( &l );
				RingBuffer<T,ELT>::pop();
			}
		}
		if ( doPop ) {
			notifyNotFull();
		}
		return true;
	}

public:
	// minfill: the number of elements that must always remain in the buffer
	RingBufferSync(unsigned ldSz, unsigned minfill = 0)
	: RingBuffer<T,ELT>( ldSz    ),
	  minfill_         ( minfill ),
	  period_          ( BsaAlias::nanoseconds( 10ULL*365ULL*3600ULL*24ULL*1000000000ULL ) ),
	  timeout_         ( BsaAlias::Clock::now() + period_                                  )
	{
		if ( minfill >= (unsigned)(1<<ldSz) ) {
			throw std::runtime_error( std::string("Requested minfill too big") );
		}
	}

	// minfill: the number of elements that must always remain in the buffer
	RingBufferSync(unsigned ldSz, const ELT &ini, unsigned minfill = 0)
	: RingBuffer<T,ELT>( ldSz, ini ),
	  minfill_         ( minfill   ),
	  period_          ( BsaAlias::nanoseconds( 10ULL*365ULL*3600ULL*24ULL*1000000000ULL ) ),
	  timeout_         ( BsaAlias::Clock::now() + period_                                  )
	{
		if ( minfill >= (unsigned)(1<<ldSz) ) {
			throw std::runtime_error( std::string("Requested minfill too big") );
		}
	}


	bool push_back(ELT v, bool doBlock = true)
	{
	bool doNotify;
		{
		Lock l( mtx_ );
			if ( doBlock ) {
				while ( RingBuffer<T,ELT>::full() ) {
					not_full_.wait( l );
				}
			} else {
				if ( RingBuffer<T,ELT>::full() ) {
					return false;
				}
			}
			RingBuffer<T,ELT>::push_back( v );

			finalizePush( &l );

			doNotify = checkMinFilled();
		}
		if ( doNotify )
			notifyMinFilled();
		return true;
	}

	void wait()
	{
	Lock l( mtx_ );
		while ( ! checkMinFilled() ) {
			minfilled_.wait( l );
		}
	}

	bool wait_until(BsaAlias::Time &timeout)
	{
	Lock l( mtx_ );
		while ( ! checkMinFilled() ) {
			if ( BsaAlias::cv_status::timeout == minfilled_.wait_until( l, timeout ) ) {
				return checkMinFilled();
			}
		}
		return true;
	}

	bool tryWait()
	{
		return wait( 0, false, false );
	}

	bool pop(T *pv, bool doBlock = true)
	{
		return wait( pv, doBlock, true );
	}

	void pop()
	{
		{
		Lock l( mtx_ );
			while ( ! checkMinFilled() ) {
				minfilled_.wait( l );
			}
			finalizePop( &l );
			RingBuffer<T,ELT>::pop();
		}
		notifyNotFull();
	}

	void
	lockedWork( void (*fn)(RingBufferSync<T,ELT> *me, void *closure), void *closure )
	{
	Lock l( mtx_ );
		fn( this, closure );
	}

	virtual void
	processItem(T *pitem)
	{
	}

	virtual void
	setPeriod(const BsaAlias::Period &period)
	{
		period_ = period;
	}

	virtual BsaAlias::Period
	getPeriod()
	{
		return period_;
	}

	virtual void
	setTimeout(const BsaAlias::Time &abs_timeout)
	{
		timeout_ = abs_timeout;
	}

	virtual void
	timeout()
	{
		throw std::runtime_error("RingBufferSync::timeout must be overridden");
	}

	void
	process()
	{
		while ( ! wait_until( timeout_ ) ) {
			timeout();
			timeout_ += period_;
		}

		T &item( RingBuffer<T,ELT>::front() );

		processItem( &item );

		pop();
	}

	// thread function
	static void
	processLoop(void *arg)
	{
	RingBufferSync<T,ELT> *thebuf = (RingBufferSync<T,ELT>*)arg;

		thebuf->setTimeout( BsaAlias::Clock::now() + thebuf->period_ ); 
		while ( 1 ) {
			thebuf->process();
		}
	}
};

#endif
