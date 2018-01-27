#ifndef BSA_FREE_LIST_H
#define BSA_FREE_LIST_H

#include <BsaAlias.h>
#include <stdio.h>

template <std::size_t BLKSZ>
class BsaFreeList {
private:
	std::size_t     numBlks_;
	std::size_t     maxBlks_;
	void           *lstHead_;
	BsaAlias::mutex lstMutx_;

	void *
	pluck()
	{
	void *rval;
		rval     = lstHead_;
		lstHead_ = *static_cast<void**>(lstHead_);
		return rval;
	}

	BsaFreeList(const BsaFreeList&);
	BsaFreeList &operator=(const BsaFreeList&);

	static const std::size_t MAX_UNLIMITED = -1;

public:
	BsaFreeList(std::size_t maxBlks = MAX_UNLIMITED)
	: numBlks_(       0 ),
	  maxBlks_( maxBlks ),
	  lstHead_(    NULL )
	{
	}

	void *alloc(std::size_t sz)
	{
		if ( sz > BLKSZ ) {
			throw std::bad_alloc();
		}
		BsaAlias::Guard lg( lstMutx_ );
		if ( lstHead_ ) {
			return pluck();
		}
		if ( numBlks_ == maxBlks_ ) {
			throw std::bad_alloc();
		}

		void *rval;

		rval = ::operator new ( BLKSZ );

		numBlks_++;
		return rval;
	}

	void free(void *p)
	{
	BsaAlias::Guard lg( lstMutx_ );
		* static_cast<void**>( p ) = lstHead_;
		lstHead_                   = p;
	}

	~BsaFreeList()
	{
	BsaAlias::Guard lg( lstMutx_ );
		printf("BsaFreeList had accumulated %ld items\n", (long)numBlks_);
		while ( lstHead_ ) {
			::operator delete( pluck() );
		}
	}

	// all allocations of the same size use the same pool
	static BsaFreeList *thePod()
	{
		static BsaFreeList thePod_;
		return &thePod_;
	}
};

template <typename T>
struct BsaPoolAllocator {

	typedef   T       value_type;

	typedef  BsaAlias::shared_ptr<T> SP;

	BsaPoolAllocator()
	{
	}

	template <typename U>
	BsaPoolAllocator(const BsaPoolAllocator<U>& o)
	{
	}

	T*
	allocate( std::size_t nelms, const void *hint = 0 )
	{
		if ( nelms != 1 ) {
			throw std::bad_alloc();
		}
		return static_cast<T*>( BsaFreeList<sizeof(T)>::thePod()->alloc( sizeof(T) ) );
	}

	void
	deallocate(T *p, std::size_t nelms)
	{
		if ( nelms != 1 ) {
			throw std::bad_alloc();
		}
		BsaFreeList<sizeof(T)>::thePod()->free( static_cast<void*>( p ) );
	}

	template <typename U>
	struct rebind {
		typedef BsaPoolAllocator<U> other;
	};

	static SP
	make()
	{
		return BsaAlias::shared_ptr<T>( BsaAlias::allocate_shared<T>( BsaPoolAllocator<T>() ) );
	}

	template <typename A0>
	static SP
	make(A0 a0)
	{
		return SP( BsaAlias::allocate_shared<T>( BsaPoolAllocator<T>(), a0 ) );
	}

	template <typename A0, typename A1>
	static BsaAlias::shared_ptr<T>
	make(A0 a0, A1 a1)
	{
		return SP( BsaAlias::allocate_shared<T>( BsaPoolAllocator<T>(), a0, a1 ) );
	}

	template <typename A0, typename A1, typename A2>
	static BsaAlias::shared_ptr<T>
	make(A0 a0, A1 a1, A2 a2)
	{
		return SP( BsaAlias::allocate_shared<T>( BsaPoolAllocator<T>(), a0, a1, a2 ) );
	}
		
};

#endif
