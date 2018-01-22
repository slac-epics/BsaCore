#ifndef BSA_RING_BUFFER_SYNC_H
#define BSA_RING_BUFFER_SYNC_H

#include <RingBuffer.h>
#include <condition_variable>
#include <mutex>

// Ring buffer with synchronization (producer sleeps on full,
// consumer on empty).
template <typename T, typename ELT = const T&>
class RingBufferSync : public RingBuffer<T, ELT> {
private:
	unsigned                 minfill_;
	std::mutex               mtx_;
	std::condition_variable  minfilled_;
	std::condition_variable  not_full_;

protected:
	typedef std::unique_lock<std::mutex> Lock;

	std::mutex               & getMtx()
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

	virtual void finalizePush()
	{
	}

	virtual void finalizePop()
	{
	}

	bool wait(T *pv, bool doBlock, bool doPop)
	{
	std::unique_lock<std::mutex> l( mtx_ );
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
			finalizePop();
			RingBuffer<T,ELT>::pop();
			l.unlock();
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
			throw std::runtime_error("Requested minfill too big");
		}
	}

	bool push_back(ELT v, bool doBlock = true)
	{
	std::unique_lock<std::mutex> l( mtx_ );
	bool                         doNotify;
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

		finalizePush();

		doNotify = checkMinFilled();
		l.unlock();
		if ( doNotify )
			notifyMinFilled();
		return true;
	}

	void wait()
	{
		wait( 0, true, false );
	}

	bool tryWait()
	{
		return wait( 0, false, false );
	}

	bool pop(T *pv, bool doBlock = true)
	{
		return wait( pv, doBlock, true );
	}

	void
	lockedWork( void (*fn)(RingBufferSync<T,ELT> *me, void *closure), void *closure )
	{
	std::unique_lock<std::mutex> l( mtx_ );
		fn( this, closure );
	}
};

#endif
