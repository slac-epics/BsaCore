#include <BsaApi.h>
#include <BsaTimeStamp.h>
#include <BsaCore.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <atomic>
#include <mutex>
#include <vector>
#include <math.h>
#include <pthread.h>

#define EDEF_MAX 8

static long remaining;
static long tested;

struct EDEF {
private:
	BsaTimeStamp tsInit_;
	uint64_t     period_;
	uint64_t     navg_;
	uint64_t     nvals_;
	BsaTimeStamp tsFini_;
	uint64_t     checked_;
	unsigned     sevr_, stat_;
	unsigned     id_;
public:
	EDEF()
	{
	}

	EDEF(unsigned id, BsaTimeStamp tsInit, unsigned period, unsigned navg, unsigned nvals)
	: tsInit_  ( tsInit  ),
	  period_  ( period  ),
	  navg_    ( navg    ),
	  nvals_   ( nvals   ),
	  tsFini_  ( tsInit_ + (uint32_t)(period_*nvals_*navg_) ),
	  checked_ ( 0       ),
	  sevr_    ( 0       ),
	  stat_    ( 0       ),
	  id_      ( id      )
	{
	}


	BsaTimeStamp tsInit() const { return tsInit_; }
	uint64_t     period() const { return period_; }
	uint64_t     nvals () const { return nvals_;  }
	uint64_t     navg  () const { return navg_;   }
	BsaTimeStamp tsFini() const { return tsFini_; }

	unsigned     id()     const { return id_;     }
};

class PatternTestGen : public BsaThread {
private:
	BsaTimingData     pattern_;
	BsaTimingCallback callback_;
	void             *userPvt_;

	typedef std::unique_lock<std::mutex> Lock;

	std::mutex        mtx_;

	EDEF              edefs_[EDEF_MAX];

public:
	PatternTestGen();

	static void timerLoop(void *me);

	virtual void
	run()
	{
		timerLoop(this);
	}

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
	if ( edefs_[idx].tsInit() >= pattern_.timeStamp && edefs_[idx].tsFini() >= pattern_.timeStamp ) {
		throw EdefActive();
	}
	if ( edef.tsInit() < pattern_.timeStamp ) {
		throw EdefTooOld();
	}
	edefs_[idx] = edef;
}

PatternTestGen::PatternTestGen()
: BsaThread ( "timingGen" ),
  callback_ ( 0 ),
  userPvt_  ( 0 )
{
	memset( &pattern_, 0, sizeof(pattern_) );
	memset( edefs_, 0, sizeof(edefs_) );
	pattern_.pulseId        = 1;
	pattern_.timeStamp.nsec = 1;
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
		epicsTimeStamp now = me->getCurrentTime();
		v = now.nsec;
		if ( BSA_StoreData(ch, now, v, 0, 0) ) {
			printf("Store data FAILED (pid %llu)\n", (unsigned long long) me->pattern_.pulseId);	
		}
#ifdef VERBOSE
		ts.tv_nsec = 100*MS;
#else
		ts.tv_nsec = 1*MS;
#endif
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
		if ( edefs_[edef].tsInit() <= pattern_.timeStamp && edefs_[edef].tsFini() >= pattern_.timeStamp ) {
			uint64_t d = (BsaTimeStamp)pattern_.timeStamp - edefs_[edef].tsInit();
			if ( d == 0 ) {
				pattern_.edefInitMask |= msk;
			} else {
				if ( d % edefs_[edef].period() == 0 ) {
					pattern_.edefActiveMask |= msk;
					if ( d % (edefs_[edef].period() * edefs_[edef].navg()) == 0 ) {
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
#ifdef VERBOSE
EDEF *edef = (EDEF *)closure;
	printf("BSA INIT (on channel %s, EDEF %d)\n", BSA_GetChannelId( ch ), edef->id());
#endif
}

static void edefResult(BsaChannel ch, BsaResult results, unsigned numResults, void *closure)
{
EDEF *edef = (EDEF *)closure;
unsigned long i;
unsigned long long v = results[0].pulseId;
#ifdef VERBOSE
	printf("BSA RESULT (on channel %s, EDEF %d)\n", BSA_GetChannelId( ch ), edef->id());
	printf("  avg %g\n",  results[0].avg);
	printf("  rms %g\n",  results[0].rms);
	printf("  cnt %lu\n", results[0].count);
	printf("  mis %lu\n", results[0].missed);
	printf("  pid %llu\n", v );
	printf("  sev %u\n",  results[0].sevr);
	printf("  sta %u\n",  results[0].stat);
#endif

	if ( results[0].count + results[0].missed != edef->navg() ) {
		printf("EDEF: %u PID: %llu, count %lu, missed %lu, navg %lu\n", edef->id(), v, results[0].count, results[0].missed, edef->navg());
		throw std::runtime_error("Test FAILED -- count + missed != navg");
	}

	double avg = 0.0;
	for ( i = 0; i < results[0].count; i++ ) {
		avg += (double)v;
		v = v - edef->period();
	}
	avg /= (double)i;

	double rms = 0.0;
	double dif;
	v = (unsigned long long)results[0].pulseId;
	for ( i = 0; i < results[0].count; i++ ) {
		dif  = ((double)v - avg);
		rms += dif*dif;
		v = v - edef->period();
	}
	rms = ::sqrt(rms/(double)i);

	if ( abs(avg - results[0].avg) > 1.0E-10 ) {
		throw std::runtime_error("Test FAILED -- AVG mismatch");
	}

	if ( abs(rms - results[0].rms) > 1.0E-10 ) {
		throw std::runtime_error("Test FAILED -- RMS mismatch");
	}

	if ( ++tested == remaining ) {
		printf("%ld results tested; SUCCESS\n", tested);
		theGen()->kill();
	}
}

static void edefAbort(BsaChannel ch, int status, void *closure)
{
#ifdef VERBOSE
	printf("BSA ABORT, status %d (on channel %s)\n", status, BSA_GetChannelId( ch ));
#endif
}

static BsaSimpleDataSinkStruct dutSink = {
	OnInit:   edefInit,
	OnResult: edefResult,
	OnAbort:  edefAbort
};

#define EDEF_0 0
#define EDEF_1 1
#define EDEF_2 2

extern "C" unsigned BSA_LD_PATTERNBUF_SZ;

int
main()
{
	BSA_LD_PATTERNBUF_SZ = 4;
std::vector<EDEF> edefs;
unsigned i;
	edefs.push_back( EDEF(EDEF_0, 10, 2,  1, 130) );
	edefs.push_back( EDEF(EDEF_1, 13, 3,  3, 130) );
	edefs.push_back( EDEF(EDEF_2, 17, 29, 4, 13)  );

BsaChannel ch = BSA_CreateChannel("CH1");

	remaining = 0;
	tested    = 0;
	for ( i=0; i<edefs.size(); i++ ) {
		remaining += edefs[i].nvals();
		if ( BSA_AddSimpleSink( ch, EDEF_0 + i, &dutSink, &edefs[i] ) ) {
			throw std::runtime_error("TEST FAILED (unable to attach sink)");
		}
		theGen()->setEdef( edefs[i], EDEF_0 + i );
	}

	BSA_TimingCallbackRegister();

	theGen()->start();

	theGen()->join();

	printf("Leaving\n");
}
