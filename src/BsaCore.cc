#include <BsaCore.h>
#include <BsaAlias.h>
#include <string.h>
#include <stdio.h>

#undef  BSA_CORE_DEBUG

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

BsaCore::BsaCore(unsigned pbufLdSz, unsigned pbufMinFill)
: BsaThread( "BsaCore" ),
  PatternBuffer( pbufLdSz, pbufMinFill )
{
	inpBufs_.reserve(NUM_INP_BUFS);
	outBufs_.reserve(NUM_OUT_BUFS);
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
		if ( (unsigned)chid < NUM_INP_BUFS ) {
			char nam[20];
			::snprintf(nam, sizeof(nam), "IBUF%d", chid);
			inpBufs_.push_back( BsaInpBufPtr( new BsaInpBuf ( this, IBUF_SIZE_LD, nam, chid ) ) );
			inpBufs_[chid]->start();
		}
		if ( (unsigned)chid < NUM_OUT_BUFS ) {
			char nam[20];
			::snprintf(nam, sizeof(nam), "OBUF%d", chid);
			outBufs_.push_back( BsaOutBufPtr( new BsaOutBuf( this, OBUF_SIZE_LD, nam, BsaResultPtr() ) ) );
			outBufs_[chid]->start();
		}
		BsaOutBuf  *obuf = outBufs_[chid % NUM_OUT_BUFS].get();
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

#ifdef BSA_CORE_DEBUG
	printf("Entered pattern for pulse ID %llu (size %lu)\n", (unsigned long long)pattern->pulseId, (unsigned long) size());
#endif
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
		chid += NUM_INP_BUFS;
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
unsigned idx  = chid % NUM_INP_BUFS;
	// non-blocking store
	return ! inpBufs_[idx]->push_back( BsaDatum( ts, value, status, severity, chid ), false );
}

void
BsaCore::processInput(BsaDatum *pitem)
{
	channels_[ pitem->chid ]->processInput( this, pitem );
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
