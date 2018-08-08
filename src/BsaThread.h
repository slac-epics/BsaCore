#ifndef BSA_THREAD_H
#define BSA_THREAD_H

#include <BsaAlias.h>
#include <string>

class BsaThreadWrapper;

class BsaThread {
private:
	typedef BsaAlias::shared_ptr<BsaThreadWrapper>  Thread;

	static const int             DEFAULT_PRIORITY = 0;

	Thread                       tid_;
	void                       (*fun_)(void*);
	std::string                  nam_;
	int                          pri_;

public:
	BsaThread(const char *nam);

	virtual int  getPriority();

	// The pri_ field is initialized to an invalid priority.
	// If during 'start' the priority has not been set for
	// a thread by the user (or a derived class) then it is
	// set to the return value of 'getDefaultPriority()' (which
	// can be overridden by a derived class).
	virtual int  getDefaultPriority()
	{
		return DEFAULT_PRIORITY;
	}

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
