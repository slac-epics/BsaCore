#include <BsaApi.h>
#include <alarm.h>

/* Borrowed from B. Dalesio's implementation (aiRecord) */
void
BSA_CheckAlarms(double val, BsaAlarmLimits lvls, BsaStat *status, BsaSevr *severity)
{
    double  hyst, lalm;
    double  alev;
    BsaSevr asev;

    hyst = lvls->hyst;
    lalm = lvls->lalm;

    /* alarm condition hihi */
    asev = lvls->hhsv;
    alev = lvls->hihi;
    if (asev && (val >= alev || ((lalm == alev) && (val >= alev - hyst)))) {
        if ( asev > *severity ) {
			*severity  = asev;
            *status    = HIHI_ALARM;
            lvls->lalm = alev;
		}
        return;
    }

    /* alarm condition lolo */
    asev = lvls->llsv;
    alev = lvls->lolo;
    if (asev && (val <= alev || ((lalm == alev) && (val <= alev + hyst)))) {
        if ( asev > *severity ) {
			*severity  = asev;
            *status    = LOLO_ALARM;
            lvls->lalm = alev;
		}
        return;
    }

    /* alarm condition high */
    asev = lvls->hsv;
    alev = lvls->high;
    if (asev && (val >= alev || ((lalm == alev) && (val >= alev - hyst)))) {
        if ( asev > *severity ) {
			*severity  = asev;
            *status    = HIGH_ALARM;
            lvls->lalm = alev;
		}
        return;
    }

    /* alarm condition low */
    asev = lvls->lsv;
    alev = lvls->low;
    if (asev && (val <= alev || ((lalm == alev) && (val <= alev + hyst)))) {
        if ( asev > *severity ) {
			*severity  = asev;
            *status    = LOW_ALARM;
            lvls->lalm = alev;
		}
        return;
    }

    /* we get here only if val is out of alarm by at least hyst */
    lvls->lalm = val;
    return;
}
