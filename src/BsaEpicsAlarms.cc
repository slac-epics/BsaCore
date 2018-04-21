#include <BsaApi.h>
#include <alarm.h>
#include <BsaConst.h>

const BsaSevr  BsaConst::SEVR_OK      =  NO_ALARM;
const BsaSevr  BsaConst::SEVR_MIN     =  MINOR_ALARM;
const BsaSevr  BsaConst::SEVR_MAJ     =  MAJOR_ALARM;
const BsaSevr  BsaConst::SEVR_INV     =  INVALID_ALARM;

const BsaStat  BsaConst::STAT_UDF     =  UDF_ALARM;


/* Borrowed from B. Dalesio's implementation (aiRecord) */
extern "C" void
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
