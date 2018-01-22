#ifndef BSA_CHANNEL_H
#define BSA_CHANNEL_H

#include <bsaCallbackApi.h>
#include <BsaApi.h>
#include <BsaTimeStamp.h>
#include <BsaComp.h>
#include <RingBufferSync.h>
#include <PatternBuffer.h>
#include <stdint.h>
#include <mutex>

typedef signed char BsaChid;

typedef struct BsaDatum {
	double       val;
	BsaTimeStamp timeStamp;
	BsaStat      stat;
	BsaSevr      sevr;

	BsaDatum(epicsTimeStamp ts, double val, BsaStat stat, BsaSevr sevr)
	: val      ( val  ),
      timeStamp( ts   ),
      stat     ( stat ),
      sevr     ( sevr )
	{
	}
} BsaDatum;

typedef struct BsaResultItem {
	BsaEdef                 edef;
	unsigned                seq_;
	struct BsaResultStruct  result;
} BsaResultItem;

class BsaSlot {
public:
	BsaPattern               *pattern_;
	unsigned                  noChangeCnt_;
	BsaComp                   comp_;
	unsigned                  seq_;
	void                     *usrPvt_;
	BsaSimpleDataSinkStruct   callbacks_;

};

class BsaChannelImpl : public FinalizePopCallback {
public:
	static const unsigned IBUF_SIZE_LD = 10;
	static const unsigned OBUF_SIZE_LD = 10;

	static const BsaSevr  SEVR_OK      =  0;
	static const BsaSevr  SEVR_MIN     =  1;
	static const BsaSevr  SEVR_MAJ     =  2;
	static const BsaSevr  SEVR_INV     =  3;

private:
	RingBufferSync<BsaDatum>                    inpBuf_;
	RingBufferSync<BsaResultItem>               outBuf_;

	std::vector<BsaSlot>                        slots_;
	uint64_t                                    inUseMask_;

	unsigned long                               patternTooNew_;
	unsigned long                               patternTooOld_;
	unsigned long                               patternNotFnd_;

	typedef std::unique_lock<std::mutex>        Lock;
	std::mutex                                  mtx_;

	bool                                        deferred_;

	BsaChannelImpl(const BsaChannelImpl&);
	BsaChannelImpl & operator=(const BsaChannelImpl&);
	
public:
	BsaChannelImpl();

	virtual int
	storeData(epicsTimeStamp ts, double value, BsaStat status, BsaSevr severity);

	virtual void
	processInput(PatternBuffer*);

	virtual void
	processOutput();

	void
	process(BsaEdef edef, BsaPattern *pattern, BsaDatum *item);

	virtual void
	finalizePop(PatternBuffer *pbuf);

	void
	evict(PatternBuffer *pbuf, BsaPattern *pattern);

	virtual
	~BsaChannelImpl() {};
};

#if 0
class CBsaCore {
private:
	static const unsigned PBUF_SIZE_LD = 10;

	typedef std::vector<CBsaSink> SinkVec;


	RingBufferSync<BsaTimingData, PBUF_SIZE_LD> patBuf_;

	std::vector< SinkVec               >        matrix_;

	std::vector< uint64_t              >        inUse_;

	void processInp()
	{
		while (1) {
			BsaDatum       item;
			BsaTimingData  *pattern;
			uint64_t        edefMask, anyMask;
			unsigned        edefIdx;
			BsaSevr         edefSevr;

			inpBuf_.pop( &item );

			if ( (pattern = bsaTimingDataGet( item.timeStamp ) ) ) {

				SinkVec &svec( matrix_[item.chid] );

				anyMask =   pattern->edefInitMask
				          | pattern->edefActiveMask
				          | pattern->edefAvgDoneMask
				          | pattern->edefAllDoneMask
				          | pattern->edefUpdateMask;

				anyMask &= inUse_[ item.chid ];

				for ( edefMask = 1, edefIdx = 1; anyMask; anyMask &= ~edefMask, edefMask <<= 1, edefIdx++ ) {
					CBsaSink &edef( svec[edefIdx] );

					if ( (pattern->edefInitMask & edefMask) ) {
						edef.comp.reset( pattern->timeStamp );
					}

					if ( (pattern->edefActiveMask & edefMask) ) {

						if ( edef.comp.getTimeStamp() == item.timeStamp ) {
							edef.noChangeCnt_++;
						} else {

							if ( (pattern->edefMinorMask & edefMask) ) {
								edefSevr = SEVR_MIN;
							} else if ( (pattern->edefMajorMask & edefMask) ) {
								edefSevr = SEVR_MAJ;
							} else {
								edefSevr = SEVR_INV;
							}

							if ( item.severity < edefSevr ) {

								if ( item.severity > 


							}

						}
						if ( (pattern->edefAvgDoneMask & edefMask) ) {
						}
					}
				}

				bsaTimingDataPut( pattern );
			}
			
		}
	}

};
#endif
#endif
