#include <BsaComp.h>

BsaComp::BsaComp()
{
	val_.numSamples_ = 0;
}

void
BsaComp::resetAvg()
{
	val_.sum2_        = 0.0;
	val_.mean_        = 0.0;
	val_.numSamples_  = 0;
	val_.maxSevr_     = 0;
	val_.maxSevrStat_ = 0;
	val_.missing_     = 0;
}

void
BsaComp::reset(BsaTimeStamp initTime)
{
	initTs_           = initTime;	
	resetAvg();
}

void
BsaComp::miss()
{
	val_.missing_++;
}

unsigned long
BsaComp::getMissing() const
{
	return val_.missing_;
};

BsaTimeStamp
BsaComp::getTimeStamp()   const
{
	return val_.lastTs_;
}

BsaSevr
BsaComp::getMaxSevr()     const
{
	return val_.maxSevr_;
}

BsaStat
BsaComp::getMaxSevrStat() const
{
	return val_.maxSevrStat_;
}

const BsaVal &
BsaComp::getVal() const
{
	return val_;
}


void
BsaComp::addData(double x, BsaTimeStamp ts, BsaSevr sevr, BsaStat stat)
{
double d1,d2;

	/* From:
	 * M(n)  = Sum{ i=1:n, x(i) } / n          ("Mean of n samples")
     * S2(n) = Sum{ i=1:n, (x(i) - M(n))^2 }   (" n * variance of n samples")
     *
     * M(n+1)  = ( n * M(n) + x(n+1) )/(n+1)
	 *         = M(n) + (x(n+1) - M(n))/(n+1)
	 *
	 * S2(n+1) = Sum{ i=1:n+1, (x(i) - M(n+1))^2 }
	 *
	 * with 
     *       d(i,n) := x(i) - M(n)
	 *
	 * introduced in
	 *
	 *       x(i) - M(n+1) = (x(i) - M(n)) - (x(n+1) - M(n))/(n+1)
	 *
	 *       d(i,n+1)      = d(i,n) - (d(n+1,n)/(n+1))
     *
	 *
	 * S2(n+1) = Sum{ i=1:n+1, d(i,n)^2 - 2 d(i,n) d(n+1)/(n+1) + (d(n+1,n)/(n+1))^2 }
	 *
	 * where Sum{ i=1:n+1, d(i,n)^2 } = S2(n) + d(n+1,n)^2
	 *       Sum{ i=1:n+1, d(i,n)   } = Sum{ i=1:n+1, x(i)} - (n+1) M(n)
	 *                                = n M(n) + x(n+1) - (n+1) M(n)
	 *                                = x(n+1) - M(n)
	 *                                = d(n+1)
	 *       Sum{ i=1:n+1, (d(n+1,n)/(n+1))^2 } = d(n+1,n)^2 / (n+1)
	 *
	 * =>    S2(n+1) = S2(n) + d(n+1,n)^2 - d(n+1,n)^2 / (n+1)
	 *
	 * since (see above)
	 *
	 *       d(n+1,n+1) = d(n+1,n) - (d(n+1,n)/(n+1)) = d(n+1,n) (1 - 1/(n+1))
	 *
	 * =>    S2(n+1) = S2(n) + d(n+1,n) * d(n+1,n+1)
	 *
	 * (published by B.P. Welford, 1962)
	 */

	val_.lastTs_    = ts;

	val_.numSamples_++;
	d1              = x - val_.mean_;
	val_.mean_     += d1 / (double)val_.numSamples_;  
	d2              = x - val_.mean_;
	val_.sum2_     += d1 * d2;

	if ( sevr > val_.maxSevr_ ) {
		val_.maxSevr_     = sevr;
		val_.maxSevrStat_ = stat;
	}
}

unsigned long
BsaComp::getNum() const
{
	return val_.numSamples_;
}

double
BsaComp::getMean() const
{
	return getNum() ? val_.mean_ : 0./0.;
}

double
BsaComp::getSumSquares() const
{
	return val_.sum2_;
}

double
BsaComp::getPopVar() const
{
	// returns NAN if sum2_ == n_ == 0
	return val_.sum2_/(double)val_.numSamples_;
}
