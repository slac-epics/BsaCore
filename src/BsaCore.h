#ifndef BSA_CORE_H
#define BSA_CORE_H

#include <vector>
#include <memory>
#include <BsaThread.h>
#include <BsaChannel.h>
#include <RingBufferSync.h>
#include <stdio.h>

class BsaCore;
class BsaCoreFactory;

class BsaChannelNotFound {};

template <typename T> class BsaBuf : public BsaThread, public RingBufferSync<T> {
private:
	BsaCore     *pcore_;

protected:
	virtual BsaCore *getCore();

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

	virtual ~BsaBuf();

	virtual void run();

	virtual void processItem(T *pitem);
};

class BsaInpBuf : public BsaBuf<BsaDatum> {
private:
	unsigned         id_;
	epicsTimeStamp   lastTimeout_;
	BsaTimeStamp     newestPatternTimeStamp_;
public:
	static const uint64_t DFLT_PERIOD_NS = 300ULL*1000000ULL;

	BsaInpBuf(BsaCore *pcore, unsigned ldSz, const char *name, unsigned id, uint64_t period = DFLT_PERIOD_NS)
	: BsaBuf<BsaDatum> ( pcore, ldSz, name ),
	  id_              ( id                )
	{
		setPeriod( BsaAlias::nanoseconds( period ) );
	}

	unsigned getId()
	{
		return id_;
	}

	virtual void
	updatePatternTimeStamp(BsaTimeStamp ts);

	virtual bool
	checkMinFilled();


	virtual void
	run();

	virtual void
	timeout();
};

typedef BsaBuf<BsaResultPtr>  BsaOutBuf;

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

	epicsTimeStamp                          lastTimeout_;

    int                                     inpBufPriority_;
    int                                     outBufPriority_;

	uint64_t                                updateTimeoutNs_;

	BsaCore(const BsaCore &);
	BsaCore &operator=(const BsaCore&);

public:
	BsaCore(BsaCoreFactory *config);
	virtual ~BsaCore();
	BsaChannel createChannel(const char *name);
	BsaChannel findChannel(const char *name);

	virtual
	void       run();

	virtual
	void       processItem(BsaPattern*);

	void
	processInput(BsaDatum *);

	void
	processOutput(BsaResultPtr *);

	void
	dumpChannelInfo(FILE *f = stdout);

	void
	inputTimeout(BsaInpBuf *inpBuf, epicsTimeStamp *lastTimeout);

	int
	storeData(BsaChannel pchannel, epicsTimeStamp ts, double value, BsaStat status, BsaSevr severity);

	BsaChannel findChannel(BsaChid chid);

	void       pushTimingData(const BsaTimingData *pattern);
};
#endif
