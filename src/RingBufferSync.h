#ifndef BSA_RING_BUFFER_SYNC_H
#define BSA_RING_BUFFER_SYNC_H

#include <RingBuffer.h>
#include <condition_variable>
#include <mutex>

// Ring buffer with synchronization (producer sleeps on full,
// consumer on empty).
template <typename T>
class RingBufferSync : public RingBuffer<T> {
private:
	std::mutex               mtx_;
	std::condition_variable  not_empty_;
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

	void notifyNotEmpty()
	{
		not_full_.notify_one();
	}	

public:
	RingBufferSync(unsigned ldSz)
	: RingBuffer<T>( ldSz )
	{
	}

	bool pop(T *pv = 0, bool doBlock = true)
	{
	std::unique_lock<std::mutex> l( mtx_ );
		if ( doBlock ) {
			while ( RingBuffer<T>::empty() ) {
				not_empty_.wait( l );
			}
		} else {
			if ( RingBuffer<T>::empty() ) {
				return false;
			}
		}
		if ( pv ) {
			*pv = RingBuffer<T>::front();
		}
		RingBuffer<T>::pop();
		l.unlock();
		notifyNotFull();
		return true;
	}

	bool push_back(const T &v, bool doBlock = true)
	{
	std::unique_lock<std::mutex> l( mtx_ );
		if ( doBlock ) {
			while ( RingBuffer<T>::full() ) {
				not_full_.wait( l );
			}
		} else {
			if ( RingBuffer<T>::full() ) {
				return false;
			}
		}
		RingBuffer<T>::push_back( v );
		l.unlock();
		notifyNotEmpty();
		return true;
	}

	void
	lockedWork( void (*fn)(RingBufferSync<T> *me, void *closure), void *closure )
	{
	std::unique_lock<std::mutex> l( mtx_ );
		fn( this, closure );
	}
};

#endif
