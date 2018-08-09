#ifndef BSA_THREAD_H
#define BSA_THREAD_H

#include <BsaAlias.h>
#include <string>

class BsaThreadWrapper;

class BsaThread {
private:
	typedef BsaAlias::shared_ptr<BsaThreadWrapper>  Thread;

	Thread                       tid_;
	void                       (*fun_)(void*);
	std::string                  nam_;
	int                          pri_;

public:
	BsaThread(const char *nam);

	virtual int  getPriority();

	static int   getDefaultPriority();
	static int   getPriorityMin();
	static int   getPriorityMax();

	// return real priority (the call might adjust)
	virtual int  setPriority(int priority);

	virtual void run() = 0;

	virtual void kill();
	virtual void join();

	virtual void start();
	virtual void stop();

	virtual const char *getName();

	~BsaThread();
};

#endif
