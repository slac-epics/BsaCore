#ifndef BSA_CORE_H
#define BSA_CORE_H

#include <vector>
#include <memory>
#include <BsaThread.h>
#include <BsaChannel.h>
#include <RingBufferSync.h>

class BsaCore;

class BsaChannelNotFound {};

template <typename T> class BsaBuf : public BsaThread, public RingBufferSync<T> {
private:
	BsaCore     *pcore_;

public:
	BsaBuf(BsaCore *pcore, unsigned ldSz, const char *name)
	: BsaThread        ( name  ),
	  RingBufferSync<T>( ldSz  ),
      pcore_           ( pcore )
	{
	}

	BsaBuf(BsaCore *pcore, unsigned ldSz, const char *name, const T &ini)
	: BsaThread        ( name      ),
	  RingBufferSync<T>( ldSz, ini ),
      pcore_           ( pcore     )
	{
	}

	~BsaBuf();

	virtual void run();

	virtual void processItem(T *pitem);
};

typedef BsaBuf<BsaDatum>      BsaInpBuf;

typedef BsaBuf<BsaResultItem> BsaOutBuf;

class BsaCore : public BsaThread, public PatternBuffer {
private:
	static const unsigned                   NUM_INP_BUFS = 2;
	static const unsigned                   NUM_OUT_BUFS = 2;
	static const unsigned                   IBUF_SIZE_LD = 10;
	static const unsigned                   OBUF_SIZE_LD = 10;
	typedef BsaAlias::shared_ptr<BsaChannelImpl> BsaChannelPtr;
	typedef std::vector<BsaChannelPtr>      BsaChannelVec;
	typedef BsaAlias::shared_ptr<BsaInpBuf> BsaInpBufPtr;
	typedef BsaAlias::shared_ptr<BsaOutBuf> BsaOutBufPtr;

	typedef std::vector<BsaInpBufPtr>       BsaInpBufVec;
	typedef std::vector<BsaOutBufPtr>       BsaOutBufVec;

	BsaChannelVec                           channels_;

	BsaInpBufVec                            inpBufs_;
	BsaOutBufVec                            outBufs_;

	BsaCore(const BsaCore &);
	BsaCore &operator=(const BsaCore&);

public:
	BsaCore(unsigned pbufLdSz, unsigned pbufMinFill);
	~BsaCore();
	BsaChannel createChannel(const char *name);
	BsaChannel findChannel(const char *name);

	virtual
	void       run();

	virtual
	void       processItem(BsaPattern*);

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
