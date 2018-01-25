#ifndef BSA_RING_BUFFER_SYNC_H
#define BSA_RING_BUFFER_SYNC_H

#include <RingBuffer.h>

#undef  CPP11
#ifdef CPP11
#include <condition_variable>
#include <mutex>
#else
#include <BsaMutex.h>
#include <BsaCondVar.h>
#endif

// Ring buffer with synchronization (producer sleeps on full,
// consumer on empty).
template <typename T, typename ELT = const T&>
class RingBufferSync : public RingBuffer<T, ELT> {
#ifdef CPP11
protected:
	typedef std::mutex                   Mutex;
	typedef std::condition_variable      CondVar;
	typedef std::unique_lock<std::mutex> Lock;
#else
protected:
	typedef BsaMutex                     Mutex;
	typedef BsaCondVar                   CondVar;
	typedef BsaMutex::Guard              Lock;
#endif

private:
	unsigned                 minfill_;
	Mutex                    mtx_;
	CondVar                  minfilled_;
	CondVar                  not_full_;

protected:

	Mutex                    & getMtx()
	{
		return mtx_;
	}

protected:
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
	  minfill_     ( minfill )
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

	void
	process()
	{
		wait();

		T &item( RingBuffer<T,ELT>::front() );

		processItem( &item );

		pop();
	}

	// thread function
	static void
	processLoop(void *arg)
	{
	RingBufferSync<T,ELT> *thebuf = (RingBufferSync<T,ELT>*)arg;
		while ( 1 ) {
			thebuf->process();
		}
	}
};

#endif
