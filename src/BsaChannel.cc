#include <BsaChannel.h>
#include <stdexcept>

BsaChannelImpl::BsaChannelImpl()
: inpBuf_   ( IBUF_SIZE_LD ),
  outBuf_   ( OBUF_SIZE_LD ),
  inUseMask_( 0            )
{
	slots_.reserve( NUM_EDEF_MAX );
}

int
BsaChannelImpl::storeData(epicsTimeStamp ts, double value, BsaStat status, BsaSevr severity)
{
	// non-blocking store
	return !inpBuf_.push_back( BsaDatum( ts, value, status, severity ), false );
}

void
BsaChannelImpl::processInput(PatternBuffer *pbuf)
{
BsaPattern *pattern, *prevPattern, *tmpPattern;
uint64_t    msk, act;
unsigned    i;
BsaSevr     edefSevr;

	inpBuf_.wait();

	BsaDatum item( inpBuf_.front() );

	try {
		pattern = pbuf->patternGet( item.timeStamp );

		act = pattern->edefActiveMask;

		for ( i = 0, msk = 1;  act; i++, msk <<= 1, (act &= ~msk) ) {
			if ( ! (msk & act) ) {
				continue;
			}
			if ( slots_[i].comp_.getTimeStamp() == item.timeStamp ) {
				slots_[i].noChangeCnt_++;
				continue;
			}
			Lock( mtx_ );

			if ( slots_[i].pattern_) {
				prevPattern = pbuf->patternGetNext( slots_[i].pattern_, i );
			} else {
				// pattern of last computation has expired
				prevPattern = pbuf->patternGetOldest( i );
			}

			if ( ! prevPattern ) {
				// should not happen as at least 'pattern' should be found
				throw std::runtime_error("no previous pattern found");
			}

			while ( prevPattern != pattern ) {
				if ( prevPattern->edefInitMask & msk ) {
					slots_[i].comp_.reset( prevPattern->timeStamp );
					slots_[i].seq_ = 0;
					// TODO: store INIT event
				}
				if ( prevPattern->edefActiveMask & msk ) {
					slots_[i].comp_.miss();
				}
				if ( prevPattern->edefAvgDoneMask & msk ) {
					// TODO: AVG DONE
					slots_[i].comp_.resetAvg();
					slots_[i].seq_++;
				}
				tmpPattern = prevPattern;
				prevPattern = pbuf->patternGetNext( tmpPattern, i );
				pbuf->patternPut( tmpPattern );

				if ( ! prevPattern ) {
					// should not happen as at least 'pattern' should be found
					throw std::runtime_error("no previous pattern found");
				}
			}

			pbuf->patternPut( prevPattern );

			if ( pattern->edefInitMask & msk ) {
				slots_[i].comp_.reset( pattern->timeStamp );
				slots_[i].seq_ = 0;
				// TODO: store INIT event
			}

			if ( pattern->edefMinorMask & msk ) {
				edefSevr = SEVR_MIN;
			} else if ( pattern->edefMajorMask & msk ) {
				edefSevr = SEVR_MAJ;
			} else {
				edefSevr = SEVR_INV;
			}

			if ( item.sevr < edefSevr ) {
				slots_[i].comp_.addData( item.val, item.timeStamp, item.sevr, item.stat );
			}


			if ( pattern->edefAvgDoneMask & msk ) {
				// TODO: AVG_DONE
				slots_[i].comp_.resetAvg();
				slots_[i].seq_++;
			}

			if ( slots_[i].pattern_ ) {
				pbuf->patternPut( slots_[i].pattern_ );
			}

			slots_[i].pattern_ = pbuf->patternGet( pattern );
		}


		pbuf->patternPut( pattern );
	} catch (PatternTooNew &e) {
		patternTooNew_++;
	} catch (PatternExpired &e) {
		patternTooOld_++;
	} catch (PatternNotFound &e) {
		patternNotFnd_++;
	}

	inpBuf_.pop( 0 );
}

void
BsaChannelImpl::processOutput()
{
	outBuf_.wait();

	BsaResultItem &item( outBuf_.front() );

	if ( (1<<item.edef) & inUseMask_ ) {
		if ( item.seq_ == 0 ) {
			slots_[item.edef].callbacks_.OnInit( this, slots_[item.edef].usrPvt_ );
		}
		slots_[item.edef].callbacks_.OnResult( this, &item.result, 1, slots_[item.edef].usrPvt_ );
	}

	outBuf_.pop( 0 );
}


