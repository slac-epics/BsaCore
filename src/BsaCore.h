#ifndef BSA_CORE_H
#define BSA_CORE_H

#include <vector>
#include <memory>
#include <BsaChannel.h>
#include <RingBufferSync.h>

class BsaCore;

class BsaChannelNotFound {};

class BsaInpBuf : public RingBufferSync<BsaDatum> {
private:
	BsaCore     *pcore_;
public:
	BsaInpBuf(BsaCore *pcore, unsigned ldSz);

	virtual void process(BsaDatum *pitem);
};

class BsaOutBuf : public RingBufferSync<BsaResultItem> {
private:
	BsaCore     *pcore_;
public:
	BsaOutBuf(BsaCore *pcore, unsigned ldSz);

	virtual void process(BsaResultItem *pitem);
};


class BsaCore {
private:
	static const unsigned                   NUM_INP_BUFS = 2;
	static const unsigned                   NUM_OUT_BUFS = 2;
	static const unsigned                   IBUF_SIZE_LD = 10;
	static const unsigned                   OBUF_SIZE_LD = 10;
	typedef std::unique_ptr<BsaChannelImpl> BsaChannelPtr;
	typedef std::vector<BsaChannelPtr>      BsaChannelVec;
	typedef std::unique_ptr<BsaInpBuf>      BsaInpBufPtr;
	typedef std::unique_ptr<BsaOutBuf>      BsaOutBufPtr;
	typedef std::vector<BsaInpBufPtr>       BsaInpBufVec;
	typedef std::vector<BsaOutBufPtr>       BsaOutBufVec;

	BsaChannelVec                           channels_;
	PatternBuffer                           pbuf_;

	BsaInpBufVec                            inpBufs_;
	BsaOutBufVec                            outBufs_;

	BsaCore(const BsaCore &);
	BsaCore &operator=(const BsaCore&);

public:
	BsaCore(unsigned pbufLdSz, unsigned pbufMinFill);
	BsaChannel createChannel(const char *name);
	BsaChannel findChannel(const char *name);

	void       evictOldestPattern();

	void
	processInput(BsaDatum *);

	void
	processOutput(BsaResultItem *);

	int
	storeData(BsaChannel pchannel, epicsTimeStamp ts, double value, BsaStat status, BsaSevr severity);

	BsaChannel findChannel(BsaChid chid);

	void       pushTimingData(const BsaTimingData *pattern);
};
#endif
