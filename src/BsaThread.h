#ifndef BSA_THREAD_H
#define BSA_THREAD_H

#include <BsaAlias.h>
#include <string>

namespace std {
	class thread;
};

class BsaThread {
private:
	typedef BsaAlias::shared_ptr<std::thread> Thread;
	Thread                       tid_;
	void                       (*fun_)(void*);
	std::string                  nam_;

	static void  thread_fun(BsaThread *);

public:
	BsaThread(const char *nam);

	virtual void run() = 0;

	virtual void kill();
	virtual void join();

	virtual void start();
	virtual void stop();

	~BsaThread();
};

#endif
