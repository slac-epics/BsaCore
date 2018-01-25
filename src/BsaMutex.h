#ifndef BSA_MUTEX_H
#define BSA_MUTEX_H

#include <pthread.h>

class BsaMutex {
private:
	pthread_mutex_t mtx_;
	BsaMutex(const BsaMutex &);
	BsaMutex &operator=(const BsaMutex &);
public:
	BsaMutex();

	void lock();
	void unlock();

	pthread_mutex_t *get();

	~BsaMutex();

	class Guard {
	private:
		BsaMutex *m_;
		Guard(const Guard &);
		Guard &operator=(const Guard &);
	public:
		Guard(BsaMutex &m)
		: m_( &m )
		{
			m_->lock();
		}

		pthread_mutex_t *get()
		{
			return m_->get();
		}

		~Guard()
		{
			m_->unlock();
		}
	};
};

#endif
