typedef signed char BsaChid;
typedef signed char BsaEdef;

typedef struct BsaDatum {
	double       val;
	BsaTimeStamp timeStamp;
	BsaStat      status;
	BsaSevr      severity;
	BsaChid      chid;
} BsaDatum;

typedef struct BsaResultItem {
	BsaChid      chid;
	BsaEdef      edef;
	BsaResult    result;
} BsaResultItem;

class CBsaSink {
private:
	unsigned                  noChangeCnt_;
	BsaComp                   comp_;
	BsaSimpleDataSinkStruct   callbacks_;
};

class CBsaCore {
private:
	static const unsigned IBUF_SIZE_LD = 10;
	static const unsigned OBUF_SIZE_LD = 10;
	static const unsigned PBUF_SIZE_LD = 10;

	static const BsaSevr  SEVR_OK      =  0;
	static const BsaSevr  SEVR_MIN     =  1;
	static const BsaSevr  SEVR_MAJ     =  2;
	static const BsaSevr  SEVR_INV     =  3;

	typedef std::vector<CBsaSink> SinkVec;

	RingBufferSync<BsaDatum,      IBUF_SIZE_LD> inpBuf_;
	RingBufferSync<BsaResultItem, OBUF_SIZE_LD> outBuf_;

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
