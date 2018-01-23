#include <BsaCore.h>
#include <string.h>

BsaCore::BsaCore(unsigned pbufLdSz, unsigned pbufMinFill)
: pbuf_( pbufLdSz, pbufMinFill )
{
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
BsaCore::createChannel(const char *name)
{
BsaChannel found = findChannel( name );
	if ( ! found ) {
		found = new BsaChannelImpl( name );
		channels_.push_back( std::unique_ptr<BsaChannelImpl>( found ) );
	}
	return found;
}
	
void
BsaCore::pushTimingData(const BsaTimingData *pattern)
{
	pbuf_.push_back( pattern );
}

void
BsaCore::evictOldestPattern()
{
BsaPattern             *pattern;
BsaChannelVec::iterator it;

	// wait for the next pattern to age
	pbuf_.wait();

	pattern = &pbuf_.front();

	for ( it = channels_.begin(); it != channels_.end(); ++it ) {
		// evict cached pointers from all channels
		(*it)->evict( &pbuf_, pattern );
	}

	// retire
	pbuf_.pop();
}
