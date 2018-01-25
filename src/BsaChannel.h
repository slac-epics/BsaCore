#ifndef BSA_CHANNEL_H
#define BSA_CHANNEL_H

#include <bsaCallbackApi.h>
#include <BsaApi.h>
#include <BsaTimeStamp.h>
#include <BsaAlias.h>
#include <BsaComp.h>
#include <RingBufferSync.h>
#include <PatternBuffer.h>
#include <stdint.h>

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

typedef struct BsaResultItem {
	BsaChid                 chid_;
	BsaEdef                 edef_;
	unsigned                seq_;
	struct BsaResultStruct  result_;

	BsaResultItem(
		BsaChid        chid,
		BsaEdef        edef,
		unsigned       seq,
		double         avg,
		double         rms,
		unsigned long  count,
		unsigned long  missed, // # of pulses with active EDEF but no data was received
		epicsTimeStamp timeStamp,
		BsaPulseId     pulseId,
		BsaStat        stat,
		BsaSevr        sevr)
	{
		chid_             = chid;
		edef_             = edef;
		seq_              = seq;
		result_.avg       = avg;
		result_.rms       = rms;
		result_.count     = count;
		result_.missed    = missed;
		result_.timeStamp = timeStamp;
		result_.pulseId   = pulseId;
		result_.stat      = stat;
		result_.sevr      = sevr;
	}
} BsaResultItem;

class BsaSlot {
public:
	BsaPattern               *pattern_;
	unsigned                  noChangeCnt_;
	BsaComp                   comp_;
	BsaPulseId                pulseId_;
	unsigned                  seq_;
	void                     *usrPvt_;
	BsaSimpleDataSinkStruct   callbacks_;

	BsaSlot()
	: pattern_ (0)
	{
	}
};

class BsaChannelImpl : public FinalizePopCallback {
public:
	static const BsaSevr  SEVR_OK      =  0;
	static const BsaSevr  SEVR_MIN     =  1;
	static const BsaSevr  SEVR_MAJ     =  2;
	static const BsaSevr  SEVR_INV     =  3;

private:
	RingBufferSync<BsaResultItem>              *outBuf_;

	std::vector<BsaSlot>                        slots_;
	uint64_t                                    inUseMask_;

	unsigned long                               patternTooNew_;
	unsigned long                               patternTooOld_;
	unsigned long                               patternNotFnd_;

	typedef BsaAlias::Guard                     Lock;
	BsaAlias::mutex                             mtx_;
	BsaAlias::mutex                             omtx_;

	bool                                        deferred_;

	std::string                                 name_;
	BsaChid                                     chid_;

	BsaChannelImpl(const BsaChannelImpl&);
	BsaChannelImpl & operator=(const BsaChannelImpl&);

	
public:
	BsaChannelImpl(const char *name, BsaChid chid, RingBufferSync<BsaResultItem> *obuf);

	virtual void
	processInput(PatternBuffer*, BsaDatum *);

	virtual void
	processOutput(BsaResultItem *pitem);

	void
	process(BsaEdef edef, BsaPattern *pattern, BsaDatum *item);

	virtual void
	finalizePop(PatternBuffer *pbuf);

	void
	evict(PatternBuffer *pbuf, BsaPattern *pattern);

	const char *
	getName() const;

	int
	addSink(BsaEdef edef, BsaSimpleDataSink sink, void *closure);

	int
	delSink(BsaEdef edef, BsaSimpleDataSink sink, void *closure);

	BsaChid
	getChid();

	virtual
	~BsaChannelImpl();
};

#endif
