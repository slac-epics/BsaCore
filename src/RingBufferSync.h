#ifndef BSA_RING_BUFFER_SYNC_H

#include <RingBuffer.h>
#include <condition_variable>
#include <mutex>

// Ring buffer with synchronization (producer sleeps on full,
// consumer on empty).
template <typename T, unsigned ldSz>
class RingBufferSync : public RingBuffer<T, ldSz> {
private:
	std::mutex               mtx_;
	std::condition_variable  not_empty_;
	std::condition_variable  not_full_;
public:
	bool pop(T *pv = 0, bool doBlock = true)
	{
	std::unique_lock<std::mutex> l( mtx_ );
		if ( doBlock ) {
			while ( RingBuffer<T, ldSz>::empty() ) {
				not_empty_.wait( l );
			}
		} else {
			if ( RingBuffer<T, ldSz>::empty() ) {
				return false;
			}
		}
		if ( pv ) {
			*pv = RingBuffer<T, ldSz>::front();
		}
		RingBuffer<T, ldSz>::pop();
		l.unlock();
		not_full_.notify_one();
		return true;
	}

	bool push_back(T *pv, bool doBlock = true)
	{
	std::unique_lock<std::mutex> l( mtx_ );
		if ( doBlock ) {
			while ( RingBuffer<T, ldSz>::full() ) {
				not_full_.wait( l );
			}
		} else {
			if ( RingBuffer<T, ldSz>::full() ) {
				return false;
			}
		}
		RingBuffer<T, ldSz>::push_back( *pv );
		l.unlock();
		not_empty_.notify_one();
		return true;
	}

	void
	lockedWork( void (*fn)(RingBufferSync<T,ldSz> *me, void *closure), void *closure )
	{
	std::unique_lock<std::mutex> l( mtx_ );
		fn( this, closure );
	}

};

#endif
