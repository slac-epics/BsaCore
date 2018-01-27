#include <BsaChannel.h>
#include <stdexcept>
#include <math.h>
#include <stdint.h>

#undef BSA_CHANNEL_DEBUG

BsaResultPtr
BsaResultItem::alloc(BsaChid chid, BsaEdef edef)
{
	return BsaResultPtr( BsaPoolAllocator<BsaResultItem>::make(chid, edef) );
}

void
BsaResultItem::release(BsaResult r)
{
uintptr_t resultsField = reinterpret_cast<uintptr_t>( r );
uintptr_t resultsOff   = reinterpret_cast<uintptr_t>( &static_cast<BsaResultItem*>(0)->results_[0] );

BsaResultItem *basePtr = reinterpret_cast<BsaResultItem*>(resultsField - resultsOff);

	basePtr->self_.reset();
}


BsaSlot::BsaSlot(BsaChid chid, BsaEdef edef)
: pattern_   ( 0                                  ),
  work_      ( BsaResultItem::alloc( chid, edef ) ),
  currentRes_( 0                                  ),
  comp_      ( &work_->results_[currentRes_]      ),
  maxResults_( BSA_RESULTS_MAX                    )
{
}

BsaChannelImpl::BsaChannelImpl(const char *name, BsaChid chid, RingBufferSync<BsaResultPtr> *obuf)
: outBuf_   ( obuf         ),
  inUseMask_( 0            ),
  deferred_ ( false        ),
  name_     ( name         ),
  chid_     ( chid         )
{
unsigned edef;
	slots_.reserve( NUM_EDEF_MAX );
	for ( edef=0; edef<NUM_EDEF_MAX; edef++ ) {
		slots_.push_back( BsaSlot(chid, edef) );
	}
}

BsaChannelImpl::~BsaChannelImpl()
{
	printf("Bsa Channel %s vv\n", name_.c_str());
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

BsaSlot &slot( slots_[edef] );

#ifdef BSA_CHANNEL_DEBUG
printf("BsaChannelImpl::process (edef %d)\n", edef);
#endif

	if ( pattern->edefInitMask & msk ) {
#ifdef BSA_CHANNEL_DEBUG
printf("BsaChannelImpl::process (edef %d) -- INIT\n", edef);
#endif
		BsaResultPtr buf;

		// mark the init spot
		slot.work_->isIni_ = true;

		// If there is a previous result we must send it off
		if ( slot.work_->numResults_ > 0 ) {
			// swap buffers
			buf        = slot.work_;
			slot.work_ = BsaResultItem::alloc( chid_, edef );
		}

		// 'comp' still looks at the old buffer; 'reset' wraps up
		// the previous computation and initializes the next one
		// (possibly in the new buffer).
		// In any case, numResults must now be 0...

		slot.comp_.reset( pattern->timeStamp, &slot.work_->results_[0] );

		// if there was an old buffer we must send it out now...
		if ( buf ) {
			outBuf_->push_back( buf );
		}
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
				slot.comp_.addData( item->val, item->timeStamp, pattern->pulseId, item->sevr, item->stat );
			}
		} else {
#ifdef BSA_CHANNEL_DEBUG
printf("BsaChannelImpl::process (edef %d) -- ACTIVE (missed)\n", edef);
#endif
			slot.comp_.miss();
		}
	}

	if ( pattern->edefAvgDoneMask & msk ) {
#ifdef BSA_CHANNEL_DEBUG
printf("BsaChannelImpl::process (edef %d) -- AVG_DONE\n", edef);
#endif
		BsaResultPtr buf;

		if ( ++slot.work_->numResults_ >= slot.maxResults_ ) {
			// swap buffers
			buf        = slot.work_;
			slot.work_ = BsaResultItem::alloc( chid_, edef );
		}

		// 'comp' still looks at the old buffer; 'resetAvg' wraps up
		// the previous computation and initializes the next one
		// (possibly in the new buffer)

		slot.comp_.resetAvg( &slot.work_->results_[slot.work_->numResults_] );

		if ( buf ) {
			outBuf_->push_back( buf );
		}
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
BsaChannelImpl::processOutput(BsaResultPtr *pitem)
{
Lock         lg( omtx_ );
BsaResultPtr buf;

	         buf.swap( *pitem );

BsaSlot     &slot( slots_[buf->edef_] );

	if ( (1<<buf->edef_) & inUseMask_ ) {
		if ( buf->isIni_ ) {
			slot.callbacks_.OnInit( this, slot.usrPvt_ );
		}
		// since we pass the buffer to C code we must keep a shared_ptr reference.
		// We do that simply by storing a shared_ptr in the result itself.
		buf->self_ = buf;
		slot.callbacks_.OnResult( this, &buf->results_[0], buf->numResults_, slot.usrPvt_ );
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
