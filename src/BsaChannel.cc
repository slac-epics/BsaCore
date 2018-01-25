#include <BsaChannel.h>
#include <stdexcept>
#include <math.h>

#undef BSA_CHANNEL_DEBUG

BsaChannelImpl::BsaChannelImpl(const char *name, BsaChid chid, RingBufferSync<BsaResultItem> *obuf)
: outBuf_   ( obuf         ),
  inUseMask_( 0            ),
  deferred_ ( false        ),
  name_     ( name         ),
  chid_     ( chid         )
{
unsigned i;
	slots_.reserve( NUM_EDEF_MAX );
	for ( i=0; i<NUM_EDEF_MAX; i++ ) {
		slots_.push_back( BsaSlot() );
	}
}

const char *
BsaChannelImpl::getName() const
{
	return name_.c_str();
}

// called from the same thread that executes 'evict'
void
BsaChannelImpl::finalizePop(PatternBuffer *pbuf)
{
	if ( deferred_ ) {
		// we unlock the mutex but the caller still holds the pattern buffer lock.
		// Thus 'processInput'
		mtx_.unlock();
		deferred_ = false;
	}
}

// remove the oldest pattern from the buffer and
// update all affected computations
void
BsaChannelImpl::evict(PatternBuffer *pbuf, BsaPattern *pattern)
{
BsaEdef  edef;
uint64_t msk, act;

	act = pattern->edefActiveMask | pattern->edefInitMask;

#ifdef BSA_CHANNEL_DEBUG
printf("evict -- enter \n");
#endif
	Lock lg( mtx_ );
#ifdef BSA_CHANNEL_DEBUG
printf("      -- locked\n");
#endif

	for ( edef = 0, msk = 1;  act; edef++, (act &= ~msk), msk <<= 1 ) {
#ifdef BSA_CHANNEL_DEBUG
printf("Evicting edef %d (act %llu, msk %llu)\n", edef, (unsigned long long)act, (unsigned long long)msk);
#endif
		if ( pattern == slots_[edef].pattern_ ) {
#ifdef BSA_CHANNEL_DEBUG
printf("evict -- found pattern\n");
#endif
			// The pattern associated with the last computation done for this edef
			// is about to expire. The computation has been done, so there are no
			// lost data at this point but we must release the 'pattern_'.
			slots_[edef].pattern_ = 0;
			pbuf->patternPut( pattern );
#ifdef BSA_CHANNEL_DEBUG
printf("evict -- pattern put\n");
#endif
			// Keep the this slot locked until the pattern is truly gone so
			// that 'processInput()' cannot find this pattern anymore.
			// The 'PatternBuffer' waits for all references to the oldest pattern
			// being released and while waiting the PatternBuffer is (cannot) be
			// locked.
			// If we wouldn't hold 'mtx_' then 'processInput' could find this
			// pattern again and believe it to be expired since it would find a
			// 'pattern_ == 0' condition.
			deferred_ = true;
		} else if ( ! slots_[edef].pattern_ ) {
#ifdef BSA_CHANNEL_DEBUG
printf("evict -- no pattern\n");
#endif
			// the last computation on this slot was done in the past. We must
			// examine this pattern and account for it before we can evict it
			process( edef, pattern, 0 );
		} // else: nothing to do; the computation is up-to-date (= more recent than this pattern)
          //       and everything up to slots_[edef].pattern_ has been accounted for already
#ifdef BSA_CHANNEL_DEBUG
else printf("evict -- newer pattern\n");
#endif
	}

	if ( deferred_ )
		lg.release();
}

void
BsaChannelImpl::process(BsaEdef edef, BsaPattern *pattern, BsaDatum *item)
{
uint64_t msk = (1ULL<<edef);
BsaSevr  edefSevr;

#ifdef BSA_CHANNEL_DEBUG
printf("BsaChannelImpl::process (edef %d)\n", edef);
#endif

	if ( pattern->edefInitMask & msk ) {
#ifdef BSA_CHANNEL_DEBUG
printf("BsaChannelImpl::process (edef %d) -- INIT\n", edef);
#endif
		slots_[edef].comp_.reset( pattern->timeStamp );
		slots_[edef].seq_ = 0;
	}

	if ( pattern->edefActiveMask & msk ) {
		if ( item ) {
#ifdef BSA_CHANNEL_DEBUG
printf("BsaChannelImpl::process (edef %d) -- ACTIVE (adding data)\n", edef);
#endif
			if ( pattern->edefMinorMask & msk ) {
				edefSevr = SEVR_MIN;
			} else if ( pattern->edefMajorMask & msk ) {
				edefSevr = SEVR_MAJ;
			} else {
				edefSevr = SEVR_INV;
			}

			if ( item->sevr < edefSevr ) {
				slots_[edef].comp_.addData( item->val, item->timeStamp, item->sevr, item->stat );
				slots_[edef].pulseId_ = pattern->pulseId;
			}
		} else {
#ifdef BSA_CHANNEL_DEBUG
printf("BsaChannelImpl::process (edef %d) -- ACTIVE (missed)\n", edef);
#endif
			slots_[edef].comp_.miss();
		}
	}

	if ( pattern->edefAvgDoneMask & msk ) {
#ifdef BSA_CHANNEL_DEBUG
printf("BsaChannelImpl::process (edef %d) -- AVG_DONE\n", edef);
#endif
		unsigned long n = slots_[edef].comp_.getNum();
		outBuf_->push_back(
			BsaResultItem(
				chid_,
				edef,
				slots_[edef].seq_,
				slots_[edef].comp_.getMean(),
				::sqrt(slots_[edef].comp_.getPopVar()),
				n,
				slots_[edef].comp_.getMissing(),
				slots_[edef].comp_.getTimeStamp(),
				slots_[edef].pulseId_,
				slots_[edef].comp_.getMaxSevr(),
				slots_[edef].comp_.getMaxSevrStat()
			)
		);
		slots_[edef].comp_.resetAvg();
		slots_[edef].seq_++;
	}
}

void
BsaChannelImpl::processInput(PatternBuffer *pbuf, BsaDatum *pitem)
{
BsaPattern *pattern, *prevPattern, *tmpPattern;
uint64_t    msk, act;
BsaEdef     i;

	try {
		Lock lg( mtx_ );


		pattern = pbuf->patternGet( pitem->timeStamp );

#ifdef BSA_CHANNEL_DEBUG
printf("ChannelImpl::processInput -- got item (pulse id %llu)\n", (unsigned long long) pattern->pulseId);
#endif

		act = pattern->edefActiveMask;

		for ( i = 0, msk = 1;  act; i++, (act &= ~msk), msk <<= 1 ) {
			if ( ! (msk & act) ) {
#ifdef BSA_CHANNEL_DEBUG
printf("processInput(%d) -- not active\n",i);
#endif
				continue;
			}

			if ( slots_[i].comp_.getTimeStamp() == pitem->timeStamp ) {
#ifdef BSA_CHANNEL_DEBUG
printf("processInput(%d) -- no change\n",i);
#endif
				slots_[i].noChangeCnt_++;
				continue;
			}

			tmpPattern = slots_[i].pattern_;

			slots_[i].pattern_ = pbuf->patternGet( pattern );

			// tmpPattern still holds a reference count
			if ( tmpPattern ) {
#ifdef BSA_CHANNEL_DEBUG
printf("processInput(%d) -- found slot pattern (pid %llu)\n", i, tmpPattern->pulseId);
#endif
				prevPattern = pbuf->patternGetNext( tmpPattern, i );
				// release this slot's pattern; it will be updated with
				// the current one.
				pbuf->patternPut( tmpPattern );
			} else {
#ifdef BSA_CHANNEL_DEBUG
printf("processInput(%d) -- found no slot pattern\n", i);
#endif
				// pattern of last computation has expired
				prevPattern = pbuf->patternGetOldest( i );
			}

			if ( ! prevPattern ) {
				// should not happen as at least 'pattern' should be found
				throw std::runtime_error("no previous pattern found");
			}


			while ( prevPattern != pattern ) {
#ifdef BSA_CHANNEL_DEBUG
printf("processInput(%d) -- catching up (prev_pattern %llu, pattern %llu)\n", i, (unsigned long long)prevPattern->pulseId, (unsigned long long)pattern->pulseId);
#endif

				process( i, prevPattern, 0 );

				tmpPattern = prevPattern;
				prevPattern = pbuf->patternGetNext( tmpPattern, i );
				pbuf->patternPut( tmpPattern );

				if ( ! prevPattern ) {
					// should not happen as at least 'pattern' should be found
					throw std::runtime_error("no previous pattern found");
				}
			}

			pbuf->patternPut( prevPattern );

			process( i, pattern, pitem );
		}

		pbuf->patternPut( pattern );
	} catch (PatternTooNew &e) {
#ifdef BSA_CHANNEL_DEBUG
printf("ProcessInput: pattern too new (%llu)\n", (unsigned long long)pitem->timeStamp);
#endif
		patternTooNew_++;
	} catch (PatternExpired &e) {
#ifdef BSA_CHANNEL_DEBUG
printf("ProcessInput: pattern too old (%llu)\n", (unsigned long long)pitem->timeStamp);
#endif
		patternTooOld_++;
	} catch (PatternNotFound &e) {
#ifdef BSA_CHANNEL_DEBUG
printf("ProcessInput: pattern not fnd (%llu)\n", (unsigned long long)pitem->timeStamp);
#endif
		patternNotFnd_++;
	}
}

void
BsaChannelImpl::processOutput(BsaResultItem *pitem)
{
Lock      lg( omtx_ );
	if ( (1<<pitem->edef_) & inUseMask_ ) {
		if ( pitem->seq_ == 0 ) {
			slots_[pitem->edef_].callbacks_.OnInit( this, slots_[pitem->edef_].usrPvt_ );
		}
		slots_[pitem->edef_].callbacks_.OnResult( this, &pitem->result_, 1, slots_[pitem->edef_].usrPvt_ );
	}
}

int
BsaChannelImpl::addSink(BsaEdef edef, BsaSimpleDataSink sink, void *closure)
{
Lock      lg( omtx_ );
uint64_t  m = (1ULL<<edef);

	if ( edef >= NUM_EDEF_MAX ) {
		throw std::runtime_error("BsaChannelImpl::addSink() Invalid EDEF (too big)");
	}

	if ( (m & inUseMask_) ) {
		fprintf(stderr,"Multiple Sinks ATM Not Supported (channel %s, edef %d)\n", getName(), edef);
		return -1;
	}

	slots_[edef].usrPvt_    = closure;
	slots_[edef].callbacks_ = *sink;

	inUseMask_ |= m;


	return 0;
}


int
BsaChannelImpl::delSink(BsaEdef edef, BsaSimpleDataSink sink, void *closure)
{
Lock      lg( omtx_ );
uint64_t  m = (1ULL<<edef);

	if ( edef >= NUM_EDEF_MAX ) {
		throw std::runtime_error("BsaChannelImpl::delSink() Invalid EDEF (too big)");
	}

	if ( ! (m & inUseMask_) ) {
		fprintf(stderr,"Sinks Not Connected (channel %s, edef %d)\n", getName(), edef);
		return -1;
	}

	inUseMask_ &= ~m;

	return 0;
}

BsaChid
BsaChannelImpl::getChid()
{
	return chid_;
}
