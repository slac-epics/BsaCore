#ifndef BSA_CORE_H
#define BSA_CORE_H

#include <vector>
#include <memory>
#include <BsaChannel.h>

class BsaChannelNotFound {};

class BsaCore {
private:
	typedef std::unique_ptr<BsaChannelImpl> BsaChannelPtr;
	typedef std::vector<BsaChannelPtr>      BsaChannelVec;

	BsaChannelVec                           channels_;
	PatternBuffer                           pbuf_;

	BsaCore(const BsaCore &);
	BsaCore &operator=(const BsaCore&);

public:
	BsaCore(unsigned pbufLdSz, unsigned pbufMinFill);
	BsaChannel createChannel(const char *name);
	BsaChannel findChannel(const char *name);

	void       evictOldestPattern();

	void       pushTimingData(const BsaTimingData *pattern);
};
#endif
