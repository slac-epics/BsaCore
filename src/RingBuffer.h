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
	unsigned       c_;
	unsigned       m_;

	RingBuffer & operator=(const RingBuffer&);
	RingBuffer(const RingBuffer &orig);

	static const unsigned MAX_LD_SZ = 16;
public:
	typedef T        value_type;
	typedef T       &reference;
	typedef const T &const_reference;
	typedef size_t   size_type;

	RingBuffer(unsigned ldSz)
	: h_( 0         ),
	  t_( 0         ),
	  c_( (1<<ldSz) ),
	  m_( c_ - 1    )
	{
		if ( ldSz > MAX_LD_SZ )
			throw std::range_error("RingBuffer: requested size too large");
		buf_.reserve((1<<ldSz));
	}

	// E.g., a ring-buffer of shared_ptr must be
	// initialized with empty (but valid) elements
	RingBuffer(unsigned ldSz, const ELT &ini)
	: h_(0),
	  t_(0),
	  m_((1<<ldSz)-1)
	{
	unsigned i;
		if ( ldSz > MAX_LD_SZ )
			throw std::range_error("RingBuffer: requested size too large");
		for ( i = 0; i < (unsigned)(1<<ldSz); i++ ) {
			buf_.push_back( ini );
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

	unsigned capa() const
	{
		return m_ + 1;
	}

	unsigned mask() const
	{
		return m_;
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
