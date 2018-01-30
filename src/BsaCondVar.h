#ifndef BSA_COND_VAR_H
#define BSA_COND_VAR_H

#include <BsaMutex.h>
#include <BsaPosixClock.h>

class BsaCondVar {
private:
	pthread_cond_t c_;
	class Attr {
	private:
		pthread_condattr_t a_;
	public:
		Attr();
		~Attr();
		pthread_condattr_t *get();
	};
public:
	typedef enum cv_status {
		no_timeout,
		timeout
	} cv_status;

	BsaCondVar();
	~BsaCondVar();

	void
	wait(BsaMutex::Guard &g);

	cv_status
	wait_until(BsaMutex::Guard &g, const BsaPosixTime &timeout);

	void
	notify_one();
};

#endif
