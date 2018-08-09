#include <BsaApi.h>
#include <BsaCore.h>
#include <BsaCoreFactory.h>
#include <BsaAlias.h>
#include <pthread.h>
#include <string.h>
#include <BsaDebug.h>

#include <alarm.h>
#include <sched.h>
#include <math.h>

static const unsigned epicsPriMax = 99;

typedef BsaAlias::shared_ptr<BsaCoreFactory> BsaConfigPtr;

static BsaConfigPtr  theConfig_;
static BsaCorePtr    theCore_;

#ifdef BSA_CORE_DEBUG
extern "C" {
int   bsaCoreDebug     = BSA_CORE_DEBUG;
FILE *bsaCoreDebugFile = stdout;
};
#endif

static void createTheConfig()
{
	theConfig_ = BsaAlias::make_shared<BsaCoreFactory>();
}

static BsaConfigPtr theConfig()
{
static pthread_once_t once = PTHREAD_ONCE_INIT;
int    err;
	if ( (err = pthread_once( &once, createTheConfig )) ) {
		throw std::runtime_error(std::string("pthread_once failed: ") + std::string( strerror(err) ) );
	}
	return theConfig_;
}

static void createTheCore()
{
	theCore_ = theConfig()->create();
	theCore_->start();
}

static BsaCorePtr theCore()
{
static pthread_once_t once = PTHREAD_ONCE_INIT;
int    err;
	if ( (err = pthread_once( &once, createTheCore )) ) {
		throw std::runtime_error(std::string("pthread_once failed: ") + std::string( strerror(err) ) );
	}
	return theCore_;
}

static int epics2PosixPriority(unsigned epicsPri)
{
double min = (double)sched_get_priority_min( SCHED_FIFO );
double max = (double)sched_get_priority_max( SCHED_FIFO );

double pri = (max - min)/(double)epicsPriMax*((double)epicsPri) + min;
int   ipri = (int)round(pri);

	if ( ipri < min ) return min;
	if ( ipri > max ) return max;
	return ipri;
}

extern "C" int
BSA_ConfigSetLdPatternBufSz(unsigned val)
{
	if ( theCore_ )
		return -1;
	theConfig()->setLdBufSz( val );
	return 0;
}

extern "C" int
BSA_ConfigSetPatternBufPriority(unsigned val)
{
	if ( theCore_ || val > epicsPriMax )
		return -1;
	theConfig()->setPatternBufPriority( epics2PosixPriority( val ) );
	return 0;
}

extern "C" int
BSA_ConfigSetInputBufPriority(unsigned val)
{
	if ( theCore_ || val > epicsPriMax )
		return -1;
	theConfig()->setInputBufPriority( epics2PosixPriority( val ) );
	return 0;
}

extern "C" int
BSA_ConfigSetOutputBufPriority(unsigned val)
{
	if ( theCore_ || val > epicsPriMax)
		return -1;
	theConfig()->setOutputBufPriority( epics2PosixPriority( val ) );
	return 0;
}

int
BSA_ConfigSetAllPriorites(unsigned val)
{
	if ( theCore_ || val > epicsPriMax )
		return -1;
	int ppri = epics2PosixPriority( val );
	theConfig()->setPatternBufPriority( ppri );
	theConfig()->setInputBufPriority  ( ppri );
	theConfig()->setOutputBufPriority ( ppri );
	return 0;
}

extern "C" int
BSA_ConfigSetUpdateTimeoutSecs(double val)
{
	if ( theCore_ )
		return -1;
	theConfig()->setUpdateTimeoutSecs( val );
	return 0;
}

extern "C" BsaChannel
BSA_CreateChannel(
	const char *id
)
{
	return theCore()->createChannel( id );
}

extern "C" BsaChannel
BSA_FindChannel(
	const char *id
)
{
	return theCore()->findChannel( id );
}

extern "C" void
BSA_ReleaseChannel(
	BsaChannel bsaChannel
)
{
	/* Not Implemented */
}

/*
 * Get ID of a channel
 */
extern "C" const char *
BSA_GetChannelId(
	BsaChannel bsaChannel
)
{
	return bsaChannel->getName();
}

extern "C" int
BSA_StoreData(
	BsaChannel     bsaChannel,
    epicsTimeStamp timeStamp,
	double         value,
	BsaStat        status,
	BsaSevr        severity
)
{
	return theCore()->storeData(bsaChannel, timeStamp, value, status, severity);
}

/*
 * Register a sink with a BSA channel for a given EDEF index.
 *
 * Returns: status (0 == OK)
 */
extern "C" int
BSA_AddSimpleSink(
	BsaChannel        bsaChannel,
	BsaEdef           edefIndex,
	BsaSimpleDataSink sink,
	void             *closure,
	unsigned          maxResults
)
{
	return bsaChannel->addSink( edefIndex, sink, closure, maxResults );
}

extern "C" int
BSA_DelSimpleSink(
	BsaChannel        bsaChannel,
	BsaEdef           edefIndex,
	BsaSimpleDataSink sink,
	void             *closure
)
{
	return bsaChannel->delSink( edefIndex, sink, closure );
}

static void theBsaTimingCallback( void *pUserPvt, const BsaTimingData *pNewPattern)
{
BsaCore *theCore = (BsaCore*)pUserPvt;
	theCore->pushTimingData( pNewPattern );
}

extern "C" BsaTimingCallback
BSA_TimingCallbackGet()
{
	return theBsaTimingCallback;
}

static BsaCorePtr extRef_;

extern "C" int
BSA_TimingCallbackRegister(int (*registrar)(BsaTimingCallback, void*))
{
	if ( extRef_ ) {
		/* already registered */
		return -1;
	}
	extRef_ = theCore();
	return registrar( BSA_TimingCallbackGet(), extRef_.get() );
}

extern "C" int
BSA_TimingCallbackUnregister()
{
	if ( ! extRef_ )
		return -1;
	extRef_.reset();
	return 0;
}


extern "C" void
BSA_ReleaseResults(
	BsaResult results
)
{
	BsaResultItem::release( results );
}

extern "C" void
BSA_DumpChannelStats(
	BsaChannel channel,
	FILE      *f)
{
	if ( ! f ) {
		f = stdout;
	}
	if ( ! channel ) {
		theCore()->dumpChannelInfo( f );
	} else {
		channel->dump( f );
	}
}


extern "C" void
BSA_DumpPatternBuffer(FILE *f)
{
	if ( ! f ) {
		f = stdout;
	}
	theCore()->PatternBuffer::dump( f );
}
