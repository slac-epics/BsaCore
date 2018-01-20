#ifndef BSA_RING_BUFFER_H
#define BSA_RING_BUFFER_H

#include <cstddef>
#include <vector>
#include <exception>
#include <stdexcept>

// Ring-buffer WITHOUT range checking!
// Size is a power of 2.
template <typename T, typename ELT = const T &>
class RingBuffer {
private:
	std::vector<T> buf_;
	unsigned       h_;
	unsigned       t_;
	unsigned       m_;

	RingBuffer & operator=(const RingBuffer&);
public:
	typedef T        value_type;
	typedef T       &reference;
	typedef const T &const_reference;
	typedef size_t   size_type;

	RingBuffer(unsigned ldSz)
	: h_(0),
	  t_(0),
	  m_((1<<ldSz)-1)
	{
		if ( ldSz > 16 )
			throw std::range_error("RingBuffer: requested size too large");
printf("Reserving\n");
		buf_.reserve((1<<ldSz));
	}

	RingBuffer(const RingBuffer &orig)
	: buf_(orig.buf_),
	  h_  (orig.h_  ),
	  t_  (orig.t_  ),
	  m_  (orig.m_  )
	{
	unsigned i;
		buf_.reserve( orig.buf_.capacity() );
		for ( i = h_; i != t_; i++ ) {
			buf_[ i & m_ ] = orig.buf_[ i & m_ ];
		}
	}

	size_t size() const
	{
		return t_ - h_;
	}

	bool empty() const
	{
		return size() == 0;
	}

	bool full() const
	{
		return size() == m_ + 1;
	}

	unsigned head() const
	{
		return h_;
	}

	unsigned tail() const
	{
		return t_ - 1;
	}

	T & get(unsigned idx)
	{
		return buf_[ idx & m_ ];
	}


	// 0 finds the oldest item
	T & operator[](unsigned idx)
	{
		return buf_[ (h_ + idx) & m_ ];
	}

	const T &front() const
	{
		return (*this)[0];
	}

	T &front()
	{
		return (*this)[0];
	}


	const T &back() const
	{
		return (*this)[size()-1];
	}

	T &back()
	{
		return (*this)[size()-1];
	}


	void push_back(ELT val)
	{
		buf_[t_ & m_] = val;
		t_++;
	}

	void pop()
	{
		h_++;
	}
};
#endif
