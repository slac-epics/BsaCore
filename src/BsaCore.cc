#include <BsaCore.h>
#include <string.h>
#include <thread>
#include <stdio.h>

#define BSA_CORE_DEBUG

BsaInpBuf::BsaInpBuf(BsaCore *pcore, unsigned ldSz)
: RingBufferSync<BsaDatum>(ldSz),
  pcore_( pcore )
{
}

void
BsaInpBuf::process(BsaDatum *pitem)
{
	pcore_->processInput( pitem );
}

void
BsaOutBuf::process(BsaResultItem *pitem)
{
	pcore_->processOutput( pitem );
}


BsaOutBuf::BsaOutBuf(BsaCore *pcore, unsigned ldSz)
: RingBufferSync<BsaResultItem>(ldSz),
  pcore_( pcore )
{
}


BsaCore::BsaCore(unsigned pbufLdSz, unsigned pbufMinFill)
: pbuf_( pbufLdSz, pbufMinFill )
{
	inpBufs_.reserve(NUM_INP_BUFS);
	outBufs_.reserve(NUM_OUT_BUFS);
	new std::thread( evictOldestPatternLoop, this );
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
			inpBufs_.push_back( BsaInpBufPtr( new BsaInpBuf( this, IBUF_SIZE_LD ) ) );
			new std::thread( BsaInpBuf::processLoop, inpBufs_[chid % NUM_INP_BUFS].get() );
		}
		if ( (unsigned)chid < NUM_OUT_BUFS ) {
			outBufs_.push_back( BsaOutBufPtr( new BsaOutBuf( this, OBUF_SIZE_LD ) ) );
			new std::thread( BsaOutBuf::processLoop, outBufs_[chid % NUM_OUT_BUFS].get() );
		}
		BsaOutBuf  *obuf = outBufs_[chid % NUM_OUT_BUFS].get();
		           found = new BsaChannelImpl( name, chid, obuf );
		channels_.push_back( std::unique_ptr<BsaChannelImpl>( found ) );
	}
	return found;
}
	
void
BsaCore::pushTimingData(const BsaTimingData *pattern)
{
	pbuf_.push_back( pattern );
#ifdef BSA_CORE_DEBUG
	printf("Entered pattern for pulse ID %llu (size %lu)\n", (unsigned long long)pattern->pulseId, (unsigned long) pbuf_.size());
#endif
}

void
BsaCore::evictOldestPattern()
{
BsaPattern             *pattern;
BsaChannelVec::iterator it;

	// wait for the next pattern to age
	pbuf_.wait();

	pattern = &pbuf_.front();

#ifdef BSA_CORE_DEBUG
	printf("Evicting pattern for pulse ID %llu\n", (unsigned long long)pattern->pulseId);
#endif

	for ( it = channels_.begin(); it != channels_.end(); ++it ) {
		// evict cached pointers from all channels
		(*it)->evict( &pbuf_, pattern );
	}

	// retire
	pbuf_.pop();
}

int
BsaCore::storeData(BsaChannel pchannel, epicsTimeStamp ts, double value, BsaStat status, BsaSevr severity)
{
BsaChid  chid = pchannel->getChid();
unsigned idx  = chid % NUM_INP_BUFS;
	// non-blocking store
	return !inpBufs_[idx]->push_back( BsaDatum( ts, value, status, severity, chid ), false );
}

void
BsaCore::processInput(BsaDatum *pitem)
{
	channels_[ pitem->chid ]->processInput( &pbuf_, pitem );
}

void
BsaCore::processOutput(BsaResultItem *pitem)
{
	channels_[ pitem->chid_ ]->processOutput( pitem );
}
	
void
BsaCore::evictOldestPatternLoop(void *arg)
{
BsaCore *pcore = (BsaCore *)arg;
	while ( 1 ) {
		pcore->evictOldestPattern();
	}
}