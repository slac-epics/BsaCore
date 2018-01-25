#ifndef BSA_COND_VAR_H
#define BSA_COND_VAR_H

#include <BsaMutex.h>

class BsaCondVar {
private:
	pthread_cond_t c_;
public:
	BsaCondVar();
	~BsaCondVar();

	void
	wait(BsaMutex::Guard &g);

	void
	notify_one();
};

#endif
