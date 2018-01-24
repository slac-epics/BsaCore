#include <BsaApi.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <thread>
#include <atomic>
#include <mutex>

#define EDEF_MAX 8

struct EDEF {
	uint64_t pidInit;
	uint64_t pidFini;
	uint64_t period;
	uint64_t nvals;
	uint64_t navg;
};

class PatternTestGen {
private:
	BsaTimingData     pattern_;
	BsaTimingCallback callback_;
	void             *userPvt_;

	typedef std::unique_lock<std::mutex> Lock;

	std::mutex        mtx_;

	EDEF              edefs_[EDEF_MAX];

public:
	PatternTestGen();

	void start();

	static void timerLoop(void *me);

	void
	registerCallback( BsaTimingCallback cb, void *usrPvt );

	void
	setEdef(const EDEF &edef, unsigned idx);

	epicsTimeStamp
	getCurrentTime();

private:
	void tick();

};

class EdefTooOld {};
class EdefActive {};

epicsTimeStamp
PatternTestGen::getCurrentTime()
{
Lock lg(mtx_);
	return pattern_.timeStamp;
}

void
PatternTestGen::setEdef(const EDEF &edef, unsigned idx)
{
Lock lg( mtx_ );
	if ( idx >= EDEF_MAX ) {
		throw std::runtime_error("edef index too big");
	}
	if ( edefs_[idx].pidInit >= pattern_.pulseId && edefs_[idx].pidFini >= pattern_.pulseId ) {
		throw EdefActive();
	}
	if ( pattern_.pulseId >= edef.pidInit ) {
		throw EdefTooOld();
	}
	edefs_[idx] = edef;
	edefs_[idx].pidFini = edef.pidInit + edef.period*edef.nvals*edef.navg;
}

PatternTestGen::PatternTestGen()
: callback_ ( 0 ),
  userPvt_  ( 0 )
{
	memset( &pattern_, 0, sizeof(pattern_) );
	memset( edefs_, 0, sizeof(edefs_) );
	pattern_.pulseId        = 1;
	pattern_.timeStamp.nsec = 1;
}

void
PatternTestGen::start()
{
	std::thread( timerLoop, this );
}

#define MS (1000UL*1000UL)
void
PatternTestGen::timerLoop(void *arg)
{
PatternTestGen  *me = (PatternTestGen*)arg;
struct timespec ts;
BsaChannel       ch = BSA_FindChannel("CH1");
double           v  = 0.0;
	if ( ! ch ) {
		throw std::runtime_error("TEST FAILED: Unable to find CH1");
	}
	while ( 1 ) {
		me->tick();
		if ( BSA_StoreData(ch, me->getCurrentTime(), v, 0, 0) ) {
			printf("Store data FAILED (pid %llu)\n", (unsigned long long) me->pattern_.pulseId);	
		} else {
			printf("Store data SUCC   (pid %llu)\n", (unsigned long long) me->pattern_.pulseId);	
		}
		v += 1.0;
		ts.tv_nsec = 100*MS;
		ts.tv_sec  = 0;
		::nanosleep( &ts, 0 );
	}
}

void
PatternTestGen::registerCallback( BsaTimingCallback cb, void *usrPvt )
{
	userPvt_  = usrPvt;
	std::atomic_thread_fence( std::memory_order_seq_cst );
	callback_ = cb;
	
}

void
PatternTestGen::tick()
{
unsigned edef;
uint64_t msk;

	printf("tick\n");
Lock lg( mtx_ );
	pattern_.pulseId++;
	pattern_.timeStamp.nsec++;
	if ( pattern_.timeStamp.nsec >= 1000000000UL ) {
		pattern_.timeStamp.secPastEpoch++;
		pattern_.timeStamp.nsec = 0;
	}

	for ( edef = 0, msk = 1; edef < EDEF_MAX; edef++, msk <<= 1 ) {
		pattern_.edefInitMask    &= ~msk;
		pattern_.edefActiveMask  &= ~msk;
		pattern_.edefAvgDoneMask &= ~msk;
		if ( pattern_.pulseId >= edefs_[edef].pidInit && pattern_.pulseId <= edefs_[edef].pidFini ) {
			uint64_t d = pattern_.pulseId - edefs_[edef].pidInit;
			if ( d == 0 ) {
				pattern_.edefInitMask |= msk;
			} else {
				d--;
				if ( d % edefs_[edef].period == 0 ) {
					pattern_.edefActiveMask |= msk;
					if ( d % (edefs_[edef].period * edefs_[edef].navg) == 0 ) {
						pattern_.edefAvgDoneMask |= msk;
					}
				}
			}
		}
	}

	if ( callback_ ) {
		callback_( userPvt_, &pattern_ );
	}
}

static PatternTestGen *theGen()
{
static PatternTestGen me;
	return &me;
}

extern "C" int
RegisterBsaTimingCallback( BsaTimingCallback cb, void *usrPvt )
{
	theGen()->registerCallback( cb, usrPvt );

	return 0;
}

static void edefInit(BsaChannel ch, void *closure)
{
	printf("BSA INIT (on channel %s)\n", BSA_GetChannelId( ch ));
}

static void edefResult(BsaChannel ch, BsaResult results, unsigned numResults, void *closure)
{
	printf("BSA RESULT (on channel %s)\n", BSA_GetChannelId( ch ));
	printf("  avg %g\n",  results[0].avg);
	printf("  rms %g\n",  results[0].rms);
	printf("  cnt %lu\n", (unsigned long)results[0].count);
	printf("  mis %lu\n", (unsigned long)results[0].missed);
	printf("  pid %llu\n", (unsigned long long)results[0].pulseId);
	printf("  sev %u\n",  results[0].sevr);
	printf("  sta %u\n",  results[0].stat);
}

static void edefAbort(BsaChannel ch, int status, void *closure)
{
	printf("BSA ABORT, status %d (on channel %s)\n", status, BSA_GetChannelId( ch ));
}

static BsaSimpleDataSinkStruct dutSink = {
	OnInit:   edefInit,
	OnResult: edefResult,
	OnAbort:  edefAbort
};

#define EDEF_0 0

int
main()
{
EDEF e;
BsaChannel ch = BSA_CreateChannel("CH1");

    if ( BSA_AddSimpleSink( ch, EDEF_0, &dutSink, 0 ) ) {
		throw std::runtime_error("TEST FAILED (unable to attach sink)");
	}

	e.pidInit = 10;
	e.navg    = 1;
	e.nvals   = 130;
	e.period  = 2;
	theGen()->setEdef( e, 0 );
	BSA_TimingCallbackRegister();
	PatternTestGen::timerLoop( theGen() );
}
