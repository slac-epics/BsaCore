#include <BsaChannel.h>
#include <BsaConst.h>
#include <BsaDebug.h>
#include <stdexcept>
#include <math.h>
#include <stdint.h>
#include <string.h>

#define DBG(msg...) BSA_CORE_DBG(BSA_CORE_DEBUG_CHANNEL,msg)
typedef unsigned long long dull;

// ld of depth of timestamp log; disabled if 0
#define BSA_TSLOG_LD 0


BsaResultPtr
BsaResultItem::alloc(BsaChid chid, BsaEdef edef)
{
static BsaPoolAllocator<BsaResultItem> all;

	return BsaResultPtr( all.make(chid, edef) );
}

void
BsaResultItem::release(BsaResult r)
{
BsaResultItem *dummy   = static_cast<BsaResultItem*>((uintptr_t)0);

uintptr_t resultsField = reinterpret_cast<uintptr_t>( r );
uintptr_t resultsOff   = reinterpret_cast<uintptr_t>( &(dummy->results_[0]) );

BsaResultItem *basePtr = reinterpret_cast<BsaResultItem*>(resultsField - resultsOff);
	basePtr->release();
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
  lastTs_   ( 0            ),
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
	itemsStored_          = 0;
	clockSwitchOvers_     = 0;
	inpBufferDrops_       = 0;

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

	DBG("evict -- enter \n");
	Lock lg( mtx_ );
	DBG("      -- locked\n");

	for ( edef = 0, msk = 1;  act; edef++, (act &= ~msk), msk <<= 1 ) {
		DBG("Evicting edef %d, chid %d (act %llu, msk %llu)\n", edef, chid_, (unsigned long long)act, (unsigned long long)msk);
		if ( pattern == slots_[edef].pattern_ ) {
			DBG("evict -- found pattern\n");
			// The pattern associated with the last computation done for this edef
			// is about to expire. The computation has been done, so there are no
			// lost data at this point but we must release the 'pattern_'.
			slots_[edef].pattern_ = 0;
			pbuf->patternPut( pattern );
			DBG("evict -- pattern put\n");
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
			DBG("evict -- no pattern\n");
			// the last computation on this slot was done in the past. We must
			// examine this pattern and account for it before we can evict it
			process( edef, pattern, 0 );
		} else {
			// else: nothing to do; the computation is up-to-date (= more recent than this pattern)
			//       and everything up to slots_[edef].pattern_ has been accounted for already
			DBG("evict -- newer pattern\n");
		}
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

	DBG("BsaChannelImpl::process (edef %d, chid %d)\n", edef, chid_);

	if ( pattern->edefInitMask & msk ) {
		DBG("BsaChannelImpl::process (edef %d, chid %d) -- INIT\n", edef, chid_);
		BsaResultPtr buf;

		// If there is a previous result we must send it off
		if ( slot.work_->numResults_ > 0 ) {
			DBG("Pushing out %llu (new init)\n", (dull)slot.work_->results_[slot.work_->numResults_-1].pulseId);
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
			DBG("BsaChannelImpl::process (edef %d, chid %d) -- ACTIVE (adding data; %g)\n", edef, chid_, item->val);
			if ( pattern->edefMinorMask & msk ) {
				edefSevr = BsaConst::SEVR_MIN;
			} else if ( pattern->edefMajorMask & msk ) {
				edefSevr = BsaConst::SEVR_MAJ;
			} else {
				edefSevr = BsaConst::SEVR_INV;
			}

			slot.comp_.addData( item->val, item->timeStamp, pattern->pulseId, edefSevr, item->sevr, item->stat );
		} else {
			DBG("BsaChannelImpl::process (edef %d, chid %d) -- ACTIVE (missed)\n", edef, chid_);
			slot.comp_.miss(pattern->timeStamp, pattern->pulseId);
		}
		dirtyMask_ |= msk;
	}

	bool avgDone = pattern->edefAvgDoneMask & msk;
	bool update  = (pattern->edefUpdateMask | pattern->edefAllDoneMask) & msk;

	if ( avgDone || update ) {
		DBG("BsaChannelImpl::process (edef %d, chid %d) --", edef, chid_);
		if ( avgDone ) {
			DBG(" AVG_DONE (%g)", slot.work_->results_[slot.work_->numResults_].avg);
		}
		if ( pattern->edefUpdateMask & msk ) {
			DBG(" UPDATE");
		}
		if ( pattern->edefAllDoneMask & msk ) {
			DBG(" ALL_DONE");
		}
		DBG("\n");
		BsaResultPtr buf;

		if (   (   avgDone && ( ++slot.work_->numResults_ >= slot.maxResults_ ))
		    || (   update  && (   slot.work_->numResults_ >  0                )) ) {

			// swap buffers
			DBG("Pushing out edef %d, chid %d, %llu (max %d, num %d)\n", edef, chid_, (dull)slot.work_->results_[slot.work_->numResults_-1].pulseId, slot.maxResults_, slot.work_->numResults_);
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
	fprintf(f,"     # items stored in BSA                       : %lu\n", itemsStored_          );
	fprintf(f,"     # items dropped because of 'pattern too new': %lu\n", patternTooNew_        );
	fprintf(f,"     # items dropped because of 'pattern too old': %lu\n", patternTooOld_        );
	fprintf(f,"     # items dropped because of 'pattern not fnd': %lu\n", patternNotFnd_        );
	fprintf(f,"     # items dropped because of 'pattern timeout': %lu\n", timedoutPatternDrops_ );
	fprintf(f,"     # items dropped because out of order        : %lu\n", outOfOrderItems_      );
	fprintf(f,"     # items dropped because of input-buf overfl.: %lu\n", inpBufferDrops_       );
	fprintf(f,"     # clock switchovers into the past detected  : %lu\n", clockSwitchOvers_     );
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
			DBG("Timeout chid %d -- slot pattern PID %llu\n", chid_, (dull)tmpPattern->pulseId);
			prevPattern = pbuf->patternGetNext( tmpPattern, edef );
		} else {
			// pattern of last computation has expired
			prevPattern = pbuf->patternGetOldest( edef );
		}

		while ( prevPattern && (BsaTimeStamp)prevPattern->timeStamp <= (BsaTimeStamp)*lastTimeout ) {
			DBG("Timeout (edef %d, chid %d) flushing old stuff - found %llu\n", edef, chid_, (dull)prevPattern->pulseId);
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
			DBG("Pushing out (edef %d, chid %d) %llu (timeout)\n", edef, chid_, (dull)slot.work_->results_[slot.work_->numResults_-1].pulseId);
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

	if ( edef < slots_.size() ) {
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
	}

	if ( pbuf ) {
		pbuf->dump( f );
	}
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
uint64_t    msk, act;
BsaEdef     i;
bool        stored = false;

	try {
		Lock lg( mtx_ );

#if BSA_TSLOG_LD > 0
		if ( itemTs_.full() )
			itemTs_.pop();
		itemTs_.push_back( pitem->timeStamp );
#endif

		if ( pitem->timeStamp <= lastTs_ ) {
			if ( BsaTimeStamp::nsDiff( lastTs_, pitem->timeStamp ) > CLOCK_REBASE_LIMIT ) {
				/* If the 'last' time is too far into the the future then
				 * most likely some sort of clock switchover has happened.
				 * If the new source is e.g., a TPGMini then we'd not live
				 * the day when it finally would catch up.
				 * Just force the 'lastTs_' back into the past...
				 */
				clockSwitchOvers_++;
			} else {
				++outOfOrderItems_;
				return;
			}
		}

		lastTs_ = pitem->timeStamp;

		pattern = pbuf->patternGet( pitem->timeStamp );

		DBG("ChannelImpl::processInput(chid %d) -- got item (pulse id %llu), val %g\n", chid_, (dull) pattern->pulseId, pitem->val);

		act = pattern->edefActiveMask;

		for ( i = 0, msk = 1;  act; i++, (act &= ~msk), msk <<= 1 ) {
			if ( ! (msk & act) ) {
				DBG("processInput(chid %d) -- edef %d not active\n", chid_, i);
				continue;
			}

			tmpPattern = slots_[i].pattern_;

			// tmpPattern still holds a reference count
			if ( tmpPattern ) {
			DBG("processInput(chid %d) -- edef %d found slot pattern (pid %llu)\n", chid_, i, (dull)tmpPattern->pulseId);
				if ( pitem->timeStamp <= tmpPattern->timeStamp ) {
					// we can get here if the slot_[i].pattern_ got updated because
					// of a pattern timeout. In this case the slot pattern can be
					// more recent than the lastTs_...
					timedoutPatternDrops_++;
					continue;
				}
				prevPattern = pbuf->patternGetNext( tmpPattern, i );
				// release this slot's pattern; it will be updated with
				// the current one.
				pbuf->patternPut( tmpPattern );
			} else {
				DBG("processInput(chid %d) -- edef %d found no slot pattern\n", chid_, i);
				// pattern of last computation has expired
				prevPattern = pbuf->patternGetOldest( i );
			}

			slots_[i].pattern_ = pbuf->patternGet( pattern );

			if ( ! prevPattern ) {
				// should not happen as at least 'pattern' should be found
				throw std::runtime_error(
				           std::string("no previous pattern found (had a tmpPattern: ")
				         + std::string( tmpPattern ? "YES)" : "NO)" )
				);
			}


			while ( prevPattern != pattern ) {
				DBG("processInput(chid %d) -- edef %d catching up (prev_pattern %llu, pattern %llu)\n",
				    chid_,
				    i,
				    (unsigned long long)prevPattern->pulseId,
				    (unsigned long long)pattern->pulseId);

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
			stored = true;
		}

		if ( stored ) {
			itemsStored_++;
		}

		pbuf->patternPut( pattern );
	} catch (PatternTooNew &e) {
		DBG("ProcessInput(chid %d): pattern too new (%llu)\n", chid_, (unsigned long long)pitem->timeStamp);
		patternTooNew_++;
	} catch (PatternExpired &e) {
		DBG("ProcessInput(chid %d): pattern too old (%llu)\n", chid_, (unsigned long long)pitem->timeStamp);
		patternTooOld_++;
	} catch (PatternNotFound &e) {
		DBG("ProcessInput(chid %d): pattern not fnd (%llu)\n", chid_, (unsigned long long)pitem->timeStamp);
		patternNotFnd_++;
	}
}

void
BsaChannelImpl::processOutput(BsaResultPtr *pitem)
{
Lock         lg( omtx_ );
BsaResultPtr buf;
unsigned     numResults;

	         buf.swap( *pitem );

BsaSlot     &slot( slots_[buf->edef_] );

	if ( (1<<buf->edef_) & inUseMask_ ) {
		BsaSlot::SinkVec::iterator it;
		if ( buf->isInit_ ) {
			for ( it = slot.callbacks_.begin(); it != slot.callbacks_.end(); ++it ) {
				it->first.OnInit( this, &buf->initTs_, it->second );
			}
		}
		// since we pass the buffer to C code we must keep a shared_ptr reference.
		// We do that simply by storing a shared_ptr in the result itself.
		buf->self_ = buf;
		buf->refc_.fetch_add(1);

		// this can happen if a new sink with reduced 'maxResults' is connected
		// but more results are already acquired. Just clip...
		if ( (numResults = buf->numResults_) > slot.maxResults_ ) {
			numResults = slot.maxResults_;
		}
		for ( it = slot.callbacks_.begin(); it != slot.callbacks_.end(); ++it ) {
			buf->refc_.fetch_add(1);
			it->first.OnResult( this, &buf->results_[0], numResults, it->second );
		}
		buf->release();
	}
}

int
BsaChannelImpl::addSink(BsaEdef edef, BsaSimpleDataSink sink, void *closure, unsigned maxResults)
{
Lock      lg( omtx_ );
uint64_t  m = (1ULL<<edef);
unsigned  lim;

	if ( edef >= NUM_EDEF_MAX ) {
		throw std::runtime_error("BsaChannelImpl::addSink() Invalid EDEF (too big)");
	}

	// If we are attaching a new sink with a reduced 'maxResults' then
	// we may lose some data (if currently there are results beyond the new limit)
	if ( (m & inUseMask_) ) {
		lim = slots_[edef].maxResults_;
	} else {
		lim = BSA_RESULTS_MAX ;
	}

	if ( 0 == maxResults || maxResults > lim ) {
		maxResults = lim;
	}

	slots_[edef].maxResults_ = maxResults;
	slots_[edef].callbacks_.push_back( BsaSlot::Sink( *sink, closure ) );

	inUseMask_ |= m;

	return 0;
}


int
BsaChannelImpl::delSink(BsaEdef edef, BsaSimpleDataSink sink, void *closure)
{
Lock      lg( omtx_ );
uint64_t  m   = (1ULL<<edef);
bool      fnd = false;

BsaSlot::SinkVec::iterator it;

	if ( edef >= NUM_EDEF_MAX ) {
		throw std::runtime_error("BsaChannelImpl::delSink() Invalid EDEF (too big)");
	}

	if ( (m & inUseMask_) ) {
		for ( it = slots_[edef].callbacks_.begin(); it != slots_[edef].callbacks_.end(); ++it ) {
			if ( 0 == memcmp( &it->first, sink, sizeof(*sink) ) && it->second == closure ) {
				slots_[edef].callbacks_.erase( it );
				fnd = true;
				break;
			}
		}
	}

	if ( ! fnd ) {
		fprintf(stderr,"Sink Not Connected (channel %s, edef %d)\n", getName(), edef);
		return -1;
	}

	if ( slots_[edef].callbacks_.size() == 0 ) {
		slots_[edef].maxResults_ = BSA_RESULTS_MAX;
		inUseMask_              &= ~m;
	}

	return 0;
}

BsaChid
BsaChannelImpl::getChid()
{
	return chid_;
}

void
BsaChannelImpl::printResultPoolStats(FILE *f)
{
/* FIXME -- the allocator is rebound to hold the shared_ptr control block + the item
 *          and thus the size is not correct.
 *          How to figure out the correct template parameter???
	BsaFreeList<sizeof(BsaResultItem)>::thePod()->printStats( f );
 */
}
