#include <BsaChannel.h>
#include <stdexcept>
#include <math.h>
#include <stdint.h>

#undef  BSA_CHANNEL_DEBUG

// ld of depth of timestamp log; disabled if 0
#define BSA_TSLOG_LD 0

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
  dirtyMask_( 0            ),
  deferred_ ( false        ),
  name_     ( name         ),
  chid_     ( chid         ),
  itemTs_   ( BSA_TSLOG_LD )
{
unsigned edef;
	patternTooNew_        = 0;
	patternTooOld_        = 0;
	patternNotFnd_        = 0;
	numTimeouts_          = 0;
	numTimeoutFlushes_    = 0;
	noProgressTimeouts_   = 0;
	outOfOrderItems_      = 0;
	deferredCnt_          = 0;
	timedoutPatternDrops_ = 0;

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
			// being released and while waiting the PatternBuffer is (cannot be)
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

	if ( deferred_ ) {
		++deferredCnt_;
		lg.release();
	}
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

		// If there is a previous result we must send it off
		if ( slot.work_->numResults_ > 0 ) {
#if defined( BSA_CHANNEL_DEBUG )
printf("Pushing out %d (new init)\n", slot.work_->results_[slot.work_->numResults_-1].pulseId);
#endif
			// swap buffers
			buf        = slot.work_;
			slot.work_ = BsaResultItem::alloc( chid_, edef );
		}

		// 'comp' still looks at the old buffer; 'reset' wraps up
		// the previous computation and initializes the next one
		// (possibly in the new buffer).
		// In any case, numResults must now be 0...

		slot.comp_.resetAvg( &slot.work_->results_[0] );

		// mark the init spot
		slot.work_->isInit_ = true;
		slot.work_->initTs_ = pattern->timeStamp;


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

			slot.comp_.addData( item->val, item->timeStamp, pattern->pulseId, edefSevr, item->sevr, item->stat );
		} else {
#ifdef BSA_CHANNEL_DEBUG
printf("BsaChannelImpl::process (edef %d) -- ACTIVE (missed)\n", edef);
#endif
			slot.comp_.miss(pattern->timeStamp, pattern->pulseId);
		}
		dirtyMask_ |= msk;
	}

	bool avgDone = pattern->edefAvgDoneMask & msk;
	bool update  = (pattern->edefUpdateMask | pattern->edefAllDoneMask) & msk;

	if ( avgDone || update ) {
#if defined( BSA_CHANNEL_DEBUG )
printf("BsaChannelImpl::process (edef %d) --");
if ( avgDone ) {
printf(" AVG_DONE");
}
if ( pattern->edefUpdateMask & msk ) {
printf(" UPDATE");
}
if ( pattern->edefAllDoneMask & msk ) {
printf(" ALL_DONE");
}
printf("\n");
#endif
		BsaResultPtr buf;

		if (   (   avgDone && ( ++slot.work_->numResults_ >= slot.maxResults_ ))
		    || (   update  && (   slot.work_->numResults_ >  0                )) ) {

			// swap buffers
#if defined( BSA_CHANNEL_DEBUG )
printf("Pushing out %d (max %d, num %d)\n", slot.work_->results_[slot.work_->numResults_-1].pulseId, slot.maxResults_, slot.work_->numResults_);
#endif
			buf        = slot.work_;
			slot.work_ = BsaResultItem::alloc( chid_, edef );
		}

		// 'comp' still looks at the old buffer; 'resetAvg' wraps up
		// the previous computation and initializes the next one
		// (possibly in the new buffer)

		slot.comp_.resetAvg( &slot.work_->results_[slot.work_->numResults_] );

		if ( buf ) {
			dirtyMask_ &= ~msk;
			outBuf_->push_back( buf );
		}
	}
}

void
BsaChannelImpl::dump(FILE *f)
{
	fprintf(f,"BSA Channel[%d] (%s) Dump\n", chid_, getName()                                   );
	fprintf(f,"  EDEF in use mask 0x%llx\n", (unsigned long long)inUseMask_                     );
	fprintf(f,"  Counters:\n");
	fprintf(f,"     # items dropped because of 'pattern too new': %lu\n", patternTooNew_        );
	fprintf(f,"     # items dropped because of 'pattern too old': %lu\n", patternTooOld_        );
	fprintf(f,"     # items dropped because of 'pattern not fnd': %lu\n", patternNotFnd_        );
	fprintf(f,"     # items dropped because of 'pattern timeout': %lu\n", timedoutPatternDrops_ );
	fprintf(f,"     # items dropped because out of order        : %lu\n", outOfOrderItems_      );
	fprintf(f,"     # timeout ticks                             : %lu\n", numTimeouts_          );
	fprintf(f,"     # results flushed by timeouts               : %lu\n", numTimeoutFlushes_    );
	fprintf(f,"     # timeouts with no progress                 : %lu\n", noProgressTimeouts_   );
	fprintf(f,"     # deferred evicts                           : %lu\n", deferredCnt_          );
}

void
BsaChannelImpl::timeout(PatternBuffer *pbuf, epicsTimeStamp *lastTimeout)
{
Lock lg( mtx_ );
uint64_t    msk;
BsaEdef     edef;
BsaPattern *tmpPattern, *prevPattern;

	numTimeouts_++;

	for ( edef = 0, msk = 1; edef < NUM_EDEF_MAX; edef++, msk <<= 1 ) {

		BsaSlot &slot( slots_[edef] );

		// process patterns that are still in the pattern buffer
		if ( (tmpPattern = slot.pattern_) ) {
#ifdef BSA_CHANNEL_DEBUG
			printf("Timeout -- slot pattern PID %d\n", tmpPattern->pulseId);
#endif
			prevPattern = pbuf->patternGetNext( tmpPattern, edef );
		} else {
			// pattern of last computation has expired
			prevPattern = pbuf->patternGetOldest( edef );
		}

		while ( prevPattern && (BsaTimeStamp)prevPattern->timeStamp <= (BsaTimeStamp)*lastTimeout ) {
#ifdef BSA_CHANNEL_DEBUG
			printf("Timeout (edef %d) flushing old stuff - found %d\n", edef, prevPattern->pulseId);
#endif
			process( edef, prevPattern, 0 );
			if ( tmpPattern )
				pbuf->patternPut( tmpPattern );
			slot.pattern_ = prevPattern;
			tmpPattern    = prevPattern;
			prevPattern   = pbuf->patternGetNext( tmpPattern, edef );
		}
		if ( prevPattern ) {
			pbuf->patternPut( prevPattern );
		}

		if ( ! (msk & dirtyMask_) ) {
			continue;
		}

		// push finished results to the output buffer but keep the last ongoing computation
		if ( slot.work_->numResults_ > 0 ) {
#ifdef BSA_CHANNEL_DEBUG
			printf("Pushing out %d (timeout)\n", slot.work_->results_[slot.work_->numResults_-1].pulseId);
#endif
			BsaResultPtr buf   = slot.work_;
			slot.work_ = BsaResultItem::alloc( chid_, edef );

			// the 'last' result might still be computing/averaging. Copy its current contents
			// to the new buffer
			slot.comp_.copy( &slot.work_->results_[0] );

			outBuf_->push_back( buf );

			numTimeoutFlushes_++;
		} else {
			noProgressTimeouts_++;
		}

		dirtyMask_ &= ~msk;
	}
}

void
BsaChannelImpl::debug(FILE *f, PatternBuffer *pbuf, BsaTimeStamp lastTs, unsigned edef, BsaPattern *lstPattern)
{
static BsaAlias::mutex dbgMtx;

Lock lg( dbgMtx );

BsaPattern *newPattern = slots_[edef].pattern_;

	// should not happen as at least 'pattern' should be found
	epicsTimeStamp lastTsEpics = lastTs;
	fprintf(f,"Inconsistency for EDEF %d, channel %p, CHID %d, deferred %lu\n", edef, this, getChid(), deferredCnt_);
	fprintf(f,"Last Timestamp was %9lu/%9lu\n", (unsigned long)lastTsEpics.secPastEpoch, (unsigned long)lastTsEpics.nsec );
	fprintf(f,"Old Slot Pattern:\n");
	if ( lstPattern ) {
		lstPattern->dump( f, 2, 0 );
	}
	fprintf(f,"New Slot Pattern:\n");
	if ( newPattern ) {
		newPattern->dump( f, 2, 0 );
	}

	pbuf->dump( f );
#if BSA_TSLOG_LD > 0
	fprintf(f,"Item Timestamps:\n");
	for ( unsigned xx = 0; xx < itemTs_.size(); ++xx ) {
		epicsTimeStamp ts = itemTs_[xx];
		fprintf(f,"  %9lu/%9lu\n", (unsigned long)ts.secPastEpoch, (unsigned long)ts.nsec );

	}
#endif
	dump( f );
}

void
BsaChannelImpl::processInput(PatternBuffer *pbuf, BsaDatum *pitem)
{
BsaPattern *pattern, *prevPattern, *tmpPattern;
BsaPattern *lstPattern;
uint64_t    msk, act;
BsaEdef     i;

	try {
		Lock lg( mtx_ );

#if BSA_TSLOG_LD > 0
		if ( itemTs_.full() )
			itemTs_.pop();
		itemTs_.push_back( pitem->timeStamp );
#endif

		if ( pitem->timeStamp <= lastTs_ ) {
			++outOfOrderItems_;
			return;
		}

		BsaTimeStamp lastTs = lastTs_;

		lastTs_ = pitem->timeStamp;

		pattern = pbuf->patternGet( pitem->timeStamp );

#if defined( BSA_CHANNEL_DEBUG )
printf("ChannelImpl::processInput -- got item (pulse id %llu)\n", (unsigned long long) pattern->pulseId);
#endif

		act = pattern->edefActiveMask;

		for ( i = 0, msk = 1;  act; i++, (act &= ~msk), msk <<= 1 ) {
			if ( ! (msk & act) ) {
#if defined( BSA_CHANNEL_DEBUG )
printf("processInput(%d) -- not active\n",i);
#endif
				continue;
			}

			tmpPattern = slots_[i].pattern_;

			if ( (lstPattern = tmpPattern) ) {
				pbuf->patternGet( lstPattern );
			}

			slots_[i].pattern_ = pbuf->patternGet( pattern );

			// tmpPattern still holds a reference count
			if ( tmpPattern ) {
#if defined( BSA_CHANNEL_DEBUG )
printf("processInput(%d) -- found slot pattern (pid %llu)\n", i, tmpPattern->pulseId);
#endif
				prevPattern = pbuf->patternGetNext( tmpPattern, i );
				// release this slot's pattern; it will be updated with
				// the current one.
				pbuf->patternPut( tmpPattern );
			} else {
#if defined( BSA_CHANNEL_DEBUG )
printf("processInput(%d) -- found no slot pattern\n", i);
#endif
				// pattern of last computation has expired
				prevPattern = pbuf->patternGetOldest( i );
			}

			if ( ! prevPattern ) {
				// should not happen as at least 'pattern' should be found
				throw std::runtime_error(
				           std::string("no previous pattern found (had a tmpPattern: ")
				         + std::string( tmpPattern ? "YES)" : "NO)" )
				);
			}


			while ( prevPattern != pattern ) {
#if defined( BSA_CHANNEL_DEBUG )
printf("processInput(%d) -- catching up (prev_pattern %llu, pattern %llu)\n", i, (unsigned long long)prevPattern->pulseId, (unsigned long long)pattern->pulseId);
#endif

				process( i, prevPattern, 0 );

				tmpPattern = prevPattern;
				prevPattern = pbuf->patternGetNext( tmpPattern, i );
				pbuf->patternPut( tmpPattern );

				if ( ! prevPattern ) {
					// should not happen as at least 'pattern' should be found
					epicsTimeStamp lastTsEpics = lastTs;
					fprintf(stderr,"Inconsistency for EDEF %d, channel %p, CHID %d\n", (unsigned)i, this, getChid());
					fprintf(stderr,"Last Timestamp was %9lu/%9lu\n", (unsigned long)lastTsEpics.secPastEpoch, (unsigned long)lastTsEpics.nsec );
					fprintf(stderr,"Old Slot Pattern:\n");
					if ( lstPattern ) {
						lstPattern->dump( stderr, 2, 0 );
					}
					fprintf(stderr,"New Slot Pattern:\n");
					slots_[i].pattern_->dump( stderr, 2, 0 );
					
					pbuf->dump( stderr );
					throw std::runtime_error("no previous pattern found");
				}
			}

			if ( lstPattern ) {
				pbuf->patternPut( lstPattern );
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
		if ( buf->isInit_ ) {
			slot.callbacks_.OnInit( this, &buf->initTs_, slot.usrPvt_ );
		}
		// since we pass the buffer to C code we must keep a shared_ptr reference.
		// We do that simply by storing a shared_ptr in the result itself.
		buf->self_ = buf;
		slot.callbacks_.OnResult( this, &buf->results_[0], buf->numResults_, slot.usrPvt_ );
	}
}

int
BsaChannelImpl::addSink(BsaEdef edef, BsaSimpleDataSink sink, void *closure, unsigned maxResults)
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

	if ( maxResults > BSA_RESULTS_MAX )
		maxResults = BSA_RESULTS_MAX;

	slots_[edef].maxResults_ = maxResults;
	slots_[edef].usrPvt_     = closure;
	slots_[edef].callbacks_  = *sink;

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
		fprintf(stderr,"Sink Not Connected (channel %s, edef %d)\n", getName(), edef);
		return -1;
	}

	slots_[edef].maxResults_ = BSA_RESULTS_MAX;

	inUseMask_ &= ~m;

	return 0;
}

BsaChid
BsaChannelImpl::getChid()
{
	return chid_;
}
