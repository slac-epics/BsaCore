#include <BsaCore.h>
#include <BsaAlias.h>
#include <string.h>
#include <stdio.h>
#include <BsaDebug.h>
#include <BsaCoreFactory.h>

#define DBG(msg...) BSA_CORE_DBG(BSA_CORE_DEBUG_CORE,msg)
typedef unsigned long long dull;

template<>
void
BsaBuf<BsaDatum>::processItem(BsaDatum *pitem)
{
	pcore_->processInput( pitem );
}

template<>
void
BsaBuf<BsaResultPtr>::processItem(BsaResultPtr *pitem)
{
	pcore_->processOutput( pitem );
}

template<typename T>
void
BsaBuf<T>::run()
{
	BsaBuf<T>::setTimeout( BsaAlias::Clock::now() + BsaBuf<T>::getPeriod() );
	while ( 1 )
		RingBufferSync<T>::process();
}

template <typename T>
BsaCore *
BsaBuf<T>::getCore()
{
	return pcore_;
}

template<typename T>
BsaBuf<T>::~BsaBuf()
{
	// shutdown thread before tearing other resources down
	stop();
}

void
BsaInpBuf::run()
{
	// We don't have a current time yet (and the epics clock might
	// be different from the timing clock).
	// We could use 'timingGetCurBsaPattern()' from the timingAPI
	// but that's ill-defined and not implemented currently (neither
	// by tprPattern nor event).
	// Hey - let's use epicsTimeGetEvent...
	epicsTimeGetEvent( &lastTimeout_, 1 );
	BsaBuf<BsaDatum>::run();
}

void
BsaInpBuf::timeout()
{
epicsTimeStamp then = lastTimeout_;
	lastTimeout_  = newestPatternTimeStamp_;
	getCore()->inputTimeout( this, &then );
}

bool
BsaInpBuf::checkMinFilled()
{
	// hold data items in the input queue until a pattern has
	// arrived that has a timestamp equal or later than the data
	// timestamp.
	return size() > 0 && back().timeStamp <= newestPatternTimeStamp_;
}

void
BsaInpBuf::updatePatternTimeStamp(BsaTimeStamp newTs)
{
bool doNotify;
	{
	BsaAlias::Guard lg( getMtx() );
		if ( size() > 0 ) {
			BsaTimeStamp dataTs = back().timeStamp;
			doNotify = dataTs > newestPatternTimeStamp_ && dataTs <= newTs;
		} else {
			doNotify = false;
		}
		newestPatternTimeStamp_ = newTs;
	}
	if ( doNotify ) {
		notifyMinFilled();
	}
}

BsaCore::BsaCore(BsaCoreFactory *config)
: BsaThread       ( "BsaCore" ),
  PatternBuffer   ( config->getLdBufSz(), config->getMinFill() ),
  inpBufPriority_ ( config->getInputBufPriority()              ),
  outBufPriority_ ( config->getOutputBufPriority()             ),
  updateTimeoutNs_( (config->getUpdateTimeoutSecs() * 1.0E9)   ),
  numInpBufs_     ( config->getNumInpBufs()                    ),
  numOutBufs_     ( config->getNumOutBufs()                    )
{
	inpBufs_.reserve(numInpBufs_);
	outBufs_.reserve(numOutBufs_);
	setPriority( config->getPatternBufPriority() );
}

BsaCore::~BsaCore()
{
	// shutdown thread before tearing any other resources down
	stop();
printf("BSA core v\n");
}

BsaChannel
BsaCore::findChannel(const char *name)
{
BsaChannelVec::iterator it;
	for ( it = channels_.begin(); it != channels_.end(); ++it ) {
		if ( 0 == ::strcmp( (*it)->getName(), name ) ) {
			return it->get();
		}
	}
	return 0;
}

BsaChannel
BsaCore::findChannel(BsaChid chid)
{
	if ( (unsigned)chid >= channels_.size() ) {
		throw std::runtime_error("BsaCore::findChannel(BsaChid) -- channel ID too big");
	}
	return channels_[chid].get();
}

BsaChannel
BsaCore::createChannel(const char *name)
{
BsaChannel found = findChannel( name );
	if ( ! found ) {
		BsaChid     chid = channels_.size();
		if ( (unsigned)chid < numInpBufs_ ) {
			char nam[20];
			::snprintf(nam, sizeof(nam), "IBUF%d", chid);
			inpBufs_.push_back( BsaInpBufPtr( new BsaInpBuf ( this, IBUF_SIZE_LD, nam, chid, updateTimeoutNs_ ) ) );
			inpBufs_[chid]->setPriority( inpBufPriority_ );
			inpBufs_[chid]->start();
		}
		if ( (unsigned)chid < numOutBufs_ ) {
			char nam[20];
			::snprintf(nam, sizeof(nam), "OBUF%d", chid);
			outBufs_.push_back( BsaOutBufPtr( new BsaOutBuf( this, OBUF_SIZE_LD, nam, BsaResultPtr() ) ) );
			outBufs_[chid]->setPriority( outBufPriority_ );
			outBufs_[chid]->start();
		}
		BsaOutBuf  *obuf = outBufs_[chid % numOutBufs_].get();
		           found = new BsaChannelImpl( name, chid, obuf );
		channels_.push_back( BsaChannelPtr( found ) );
		addFinalizePop( found );
	}
	return found;
}

void
BsaCore::pushTimingData(const BsaTimingData *pattern)
{
BsaInpBufVec::iterator it;

	push_back( pattern );

	// Even if the pattern ends up not being stored in the ring-
	// buffer (because there are no active edefs) we still
	// notify input buffers of the arrival timestamp so that
	// data items queued in the input buffer(s) are released.

	for ( it = inpBufs_.begin(); it != inpBufs_.end(); ++it ) {
		(*it)->updatePatternTimeStamp( pattern->timeStamp );
	}

	DBG("Entered pattern for pulse ID %llu (size %lu)\n", (dull)pattern->pulseId, (unsigned long) size());
}

void
BsaCore::processItem(BsaPattern *pattern)
{
BsaChannelVec::iterator it;
	for ( it = channels_.begin(); it != channels_.end(); ++it ) {
		// evict cached pointers from all channels
		(*it)->evict( this, pattern );
	}
	if ( pattern->getRef() != 0 ) {
		throw std::runtime_error("Pattern refcount not zero when retired");
	}
}

void
BsaCore::inputTimeout(BsaInpBuf *pbuf, epicsTimeStamp *lastTimeStamp)
{
unsigned chid = pbuf->getId();
	while ( chid < channels_.size() ) {
		channels_[chid]->timeout( this, lastTimeStamp );
		chid += numInpBufs_;
	}
}


void
BsaCore::run()
{
	while ( 1 ) {
		PatternBuffer::process();
	}
}

int
BsaCore::storeData(BsaChannel pchannel, epicsTimeStamp ts, double value, BsaStat status, BsaSevr severity)
{
BsaChid  chid = pchannel->getChid();
unsigned idx  = chid % numInpBufs_;
int      rval;
	// non-blocking store
	DBG("BsaCore::storeData (chid %d), TSLOW: %x, value: %g\n", chid, ts.nsec, value);
	rval = ! inpBufs_[idx]->push_back( BsaDatum( ts, value, status, severity, chid ), false );
	if ( rval )
		pchannel->inpBufferDrops();
	return rval;
}

void
BsaCore::processInput(BsaDatum *pitem)
{
	channels_[ pitem->chid ]->processInput( this, pitem );
}

void
BsaCore::channelDebug(const char *name, FILE *f)
{
BsaChannel ch = findChannel( name );
	if ( ch ) {
		if ( !f ) {
			f = stdout;
		}
		epicsTimeStamp ts = { 0, 0 };
		ch->debug( f, this, ts, (unsigned)-1, 0 );
	}
}

void
BsaCore::processOutput(BsaResultPtr *pitem)
{
BsaChid chid = (*pitem)->chid_;
	channels_[chid]->processOutput( pitem );
}

void
BsaCore::dumpChannelInfo(FILE *f)
{
BsaChannelVec::iterator it;
	BsaChannelImpl::printResultPoolStats( f );
	for ( it = channels_.begin(); it != channels_.end(); ++it ) {
		(*it)->dump( f );
	}
}
