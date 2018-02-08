#include <BsaComp.h>
#include <math.h>

BsaComp::BsaComp(ResPtr newBuf)
{
	current_        = newBuf;
	current_->count = 0;
}

void
BsaComp::resetAvg(ResPtr newBuf)
{
	if ( current_->count > 1 ) {
		current_->rms = ::sqrt( current_->rms/(double)current_->count );
	} else if ( current_->count == 0 ) {
		current_->avg = 0./0.;
		current_->rms = 0./0.;
	}
	if ( newBuf ) {
		current_ = newBuf;
	}

	current_->rms        = 0.0;
	current_->avg        = 0.0;
	current_->count      = 0;
	current_->sevr       = 0;
	current_->stat       = 0;
	current_->missed     = 0;
}

void
BsaComp::copy(ResPtr newBuf)
{
	*newBuf  = *current_;
	current_ = newBuf;
}

void
BsaComp::miss(BsaTimeStamp ts, BsaPulseId pid)
{
	// If there is no data in the slot; still record the timestamp
	if ( 0 == current_->count ) {
		current_->timeStamp = ts;
		current_->pulseId   = pid;
	}
	current_->missed++;
}

unsigned long
BsaComp::getMissing() const
{
	return current_->missed;
};

unsigned long
BsaComp::getCount() const
{
	return current_->count;
}

BsaTimeStamp
BsaComp::getTimeStamp()   const
{
	return current_->timeStamp;
}

BsaSevr
BsaComp::getMaxSevr()     const
{
	return current_->sevr;
}

BsaStat
BsaComp::getMaxSevrStat() const
{
	return current_->stat;
}

BsaResult
BsaComp::getVal() const
{
	return current_;
}


void
BsaComp::addData(double x, BsaTimeStamp ts, BsaPulseId pid, BsaSevr edefSevr, BsaSevr sevr, BsaStat stat)
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

	if ( sevr >= edefSevr ) {
		// If there is no data in the slot; still record the timestamp
		if ( 0 == current_->count ) {
			current_->timeStamp  = ts;
			current_->pulseId    = pid;
		}
	} else {
		current_->timeStamp  = ts;
		current_->pulseId    = pid;

		current_->count++;
		d1              = x - current_->avg;
		current_->avg       += d1 / (double)current_->count;
		d2              = x - current_->avg;
		current_->rms       += d1 * d2;

		if ( sevr > current_->sevr     ) {
			current_->sevr         = sevr;
			current_->stat         = stat;
		}
	}
}
