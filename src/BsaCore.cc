#include <BsaCore.h>
#include <string.h>
#include <stdio.h>

#undef  BSA_CORE_DEBUG

template<>
void
BsaInpBuf::processItem(BsaDatum *pitem)
{
	pcore_->processInput( pitem );
}

template<>
void
BsaOutBuf::processItem(BsaResultPtr *pitem)
{
	pcore_->processOutput( pitem );
}

template<typename T>
void
BsaBuf<T>::run()
{
	while ( 1 )
		RingBufferSync<T>::process();
}

template<typename T>
BsaBuf<T>::~BsaBuf()
{
	// shutdown thread before tearing other resources down
	stop();
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
			inpBufs_.push_back( BsaInpBufPtr( new BsaInpBuf ( this, IBUF_SIZE_LD, nam ) ) );
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
	push_back( pattern );
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
