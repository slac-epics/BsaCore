#ifndef BSA_CHANNEL_H
#define BSA_CHANNEL_H

#include <bsaCallbackApi.h>
#include <BsaApi.h>
#include <BsaTimeStamp.h>
#include <BsaAlias.h>
#include <BsaComp.h>
#include <RingBufferSync.h>
#include <PatternBuffer.h>
#include <BsaFreeList.h>
#include <stdint.h>

#define BSA_RESULTS_MAX 20

typedef signed char BsaChid;

typedef struct BsaDatum {
	double       val;
	BsaTimeStamp timeStamp;
	BsaStat      stat;
	BsaSevr      sevr;
	BsaChid      chid;

	BsaDatum(epicsTimeStamp ts, double val, BsaStat stat, BsaSevr sevr, BsaChid chid)
	: val      ( val  ),
      timeStamp( ts   ),
      stat     ( stat ),
      sevr     ( sevr ),
	  chid     ( chid )
	{
	}
} BsaDatum;

struct BsaResultItem;
typedef BsaAlias::shared_ptr<BsaResultItem> BsaResultPtr;

typedef struct BsaResultItem {
	BsaResultPtr            self_;
	BsaChid                 chid_;
	BsaEdef                 edef_;
	bool                    isInit_;
	epicsTimeStamp          initTs_;
	unsigned                numResults_;
	struct BsaResultStruct  results_[BSA_RESULTS_MAX];

	BsaResultItem(
		BsaChid        chid,
		BsaEdef        edef)
	: chid_      ( chid  ),
	  edef_      ( edef  ),
	  isInit_    ( false ),
	  numResults_(    0  )
	{
	}

	// release the 'self_' reference (for use from C code!)
	static void         release(BsaResult);

	static BsaResultPtr alloc( BsaChid chid, BsaEdef edef );
} BsaResultItem;

class BsaSlot {
public:
	BsaPattern               *pattern_;
	unsigned                  noChangeCnt_;
	BsaResultPtr              work_;
	unsigned                  currentRes_;
	BsaComp                   comp_;
	void                     *usrPvt_;
	BsaSimpleDataSinkStruct   callbacks_;
	unsigned                  maxResults_;

	BsaSlot(BsaChid chid, BsaEdef edef);
};

class BsaChannelImpl : public FinalizePopCallback {
public:
	static const BsaSevr  SEVR_OK      =  0;
	static const BsaSevr  SEVR_MIN     =  1;
	static const BsaSevr  SEVR_MAJ     =  2;
	static const BsaSevr  SEVR_INV     =  3;

private:
	RingBufferSync<BsaResultPtr>               *outBuf_;

	std::vector<BsaSlot>                        slots_;
	uint64_t                                    inUseMask_;
	uint64_t                                    dirtyMask_;

	unsigned long                               patternTooNew_;
	unsigned long                               patternTooOld_;
	unsigned long                               patternNotFnd_;

	unsigned long                               numTimeouts_;
	unsigned long                               numTimeoutFlushes_;
	unsigned long                               noProgressTimeouts_;

	typedef BsaAlias::Guard                     Lock;
	BsaAlias::mutex                             mtx_;
	BsaAlias::mutex                             omtx_;

	bool                                        deferred_;

	std::string                                 name_;
	BsaChid                                     chid_;

	BsaChannelImpl(const BsaChannelImpl&);
	BsaChannelImpl & operator=(const BsaChannelImpl&);

public:
	BsaChannelImpl(const char *name, BsaChid chid, RingBufferSync<BsaResultPtr> *obuf);

	virtual void
	processInput(PatternBuffer*, BsaDatum *);

	virtual void
	processOutput(BsaResultPtr *pitem);

	void
	process(BsaEdef edef, BsaPattern *pattern, BsaDatum *item);

	virtual void
	finalizePop(PatternBuffer *pbuf);

	void
	evict(PatternBuffer *pbuf, BsaPattern *pattern);

	void
	timeout(PatternBuffer *pbuf, epicsTimeStamp *lastTimeout);

	void
	dump(FILE *f = ::stdout);

	const char *
	getName() const;

	int
	addSink(BsaEdef edef, BsaSimpleDataSink sink, void *closure, unsigned maxResults);

	int
	delSink(BsaEdef edef, BsaSimpleDataSink sink, void *closure);

	BsaChid
	getChid();

	virtual
	~BsaChannelImpl();
};

#endif
