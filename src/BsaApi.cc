#include <BsaApi.h>
#include <BsaCore.h>

static BsaCore *theCore()
{
static unsigned LD_PATTERNBUF_SZ   = 2;
static unsigned PATTERNBUF_MINFILL = (1<<(LD_PATTERNBUF_SZ - 1));
static BsaCore theCore_( LD_PATTERNBUF_SZ, PATTERNBUF_MINFILL );
	return &theCore_;
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
	void             *closure
)
{
	return bsaChannel->addSink( edefIndex, sink, closure );
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
bsaTimingCallback()
{
	return theBsaTimingCallback;
}

extern "C" int
BSA_TimingCallbackRegister()
{
	return RegisterBsaTimingCallback( bsaTimingCallback(), theCore() );
}
