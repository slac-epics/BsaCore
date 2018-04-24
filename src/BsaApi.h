#ifndef BSA_API_H
#define BSA_API_H

#include <bsaCallbackApi.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif
/*
 * Objectives:
 *     - BSA shall be a separate module/library (unbundled from timing, epics, data acq.);
 *     - interfaces to timing, data source, data sink via APIs
 *     - API should work for lcls-1 and (slow) lcls-2 timing
 *     - make migration of legacy lcls-1 BSA easy
 *     - support low-level interface for data sources (non-EPICS. E.g.,
 *       a ATCA DAQ system might receive pulse-ID/timestamp along with
 *       data)
 *     - EPICS/records layered on top of core API/implementation.
 *
 *
 * Architecture:
 *
 *
 *                                          [Data Sink ]
 *                                            ^
 *                                            | [Data Sink ]
 *                                            |   ^
 *                             Data Sink API  |   | [Data Sink ]
 *                                            |   |   ^
 *                                            |   |   |
 *                                            v   v   v
 *               -------------              ------------
 *              | Data Source | <--------> |            |
 *              | (Internal   |Data SRC API|  BSA Core  |
 *              |  TimeStamp) |    ------> |            |
 *               -------------     |        ------------
 *                                 |             ^
 *               -------------     |             | Timing API
 *              | Data Source | <--              v
 *              | (TimeStamp  |            --------------
 *              |  from EVR)  | <-------> | Timing (EVR) |
 *               -------------             --------------
 *
 * Timing API:  The timing source provides the BSA core with bsa-relevant timing
 *              information (EDEF bitmasks, timestamp, pulse-id).
 *              Because BSA is not necessarily linked to an application the
 *              interface might be represented by a callback.
 *              Alternatively, a 'no-BSA' library of stubs could be added.
 *
 * Data Source API: Data sources feed timestamped readings to the BSA core.
 *              The core then stores or discards these readings based on their
 *              unique pulseID/timestamp and qualifiers associated with the
 *              pulseID/timestamp (which were obtained via the Timing API).
 *
 * Data Sink API: Data sinks consume BSA history data; sinks are notified
 *              when e.g., the history changes or a pre-defined number of
 *              items has been produced.
 *
 * Rationale:   The sketched API lends itself to an implementation which
 *                - keeps a history of BSA-relevant timing data (obtained
 *                  from the timing API)
 *                - when a data source stores a new (timestamped) item in
 *                  BSA (via the SRC API) then the implementation can
 *                  look up the timing data associated with the data's
 *                  pulseID and perform BSA operations (averaging, storing)
 *                  when appropriate.
 *              As long as timestamping the data can be done in real-time
 *              such a BSA implementation would be rather insensitive to
 *              processing latencies in BSA itself.
 */

#include <stdint.h>

typedef uint64_t BsaPulseId;

typedef uint16_t BsaSevr;
typedef  int16_t BsaStat;
typedef   int8_t BsaEdef;

/*
 * Opaque BSA channel object (specific to one variable)
 */
typedef struct BsaChannelImpl *BsaChannel;

/*
 * Create or obtain a BsaChannel; if the same name is used for
 * a second time then the same BsaChannel is returned (with incremented
 * internal reference count, i.e., all instances returned by
 * BSA_CreateChannel() must eventually be released (BSA_ReleaseChannel)!).
 *
 * Valid Channel IDs are between 0 and the maximum supported by the
 * Timing system.
 */
BsaChannel
BSA_CreateChannel(
	const char *id
);

/*
 * Look up a channel; returns NULL if not found.
 *
 * NOTE: The reference count is incremented if the
 *       channels was found, i.e., the caller must
 *       eventually BSA_ReleaseChannel()
 */
BsaChannel
BSA_FindChannel(
	const char *id
);

/*
 * Decrement reference count and release all resources associated
 * with this channel.
 */
void
BSA_ReleaseChannel(
	BsaChannel bsaChannel
);

/*
 * Get ID of a channel
 */
const char *
BSA_GetChannelId(
	BsaChannel bsaChannel
);

/*
 * Data Source API; every time a data source has produced a
 * new item it can be stored in BSA with this call. Thus, a
 * low-level driver may store data w/o using any EPICS.
 *
 * Returns: status (0 == OK)
 */
int
BSA_StoreData(
	BsaChannel     bsaChannel,
    epicsTimeStamp timeStamp,
	double         value,
	BsaStat        status,
	BsaSevr        severity
);

/*
 * Helper Routine (which replicates EPICS' aiRecord's 'checkAlarms')
 */
typedef struct BsaAlarmLimitsStruct {
	double  lolo, low, high, hihi;
	double  lalm, hyst;
	BsaSevr llsv, lsv, hsv , hhsv;
} BsaAlarmLimitsStruct, *BsaAlarmLimits;

/*
 * This routine is intended to be fast; if
 * access to the BsaAlarmLimits needs to be
 * protected the caller can employ a spinlock
 * or mutex.
 * NOTE: 'laml' is modified by this routine.
 *       'status' and 'severity' are updated
 *       (i.e., they already must have valid
 *        content since the severity is only
 *        increased by this routine).
 */
void
BSA_CheckAlarms(double val, BsaAlarmLimits levels, BsaStat *status, BsaSevr *severity);

/*
 * Data Sink API;
 *
 * Simple first version -- assumes *external* history buffers.
 *
 * However, the separation of Src and Sink APIs also allows
 * an implementation e.g., to buffer 'AvgDone' transactions
 * internally on a queue and dispatch the callbacks of the
 * Sink API from a worker thread so that the execution
 * of 'BSA_StoreData' and 'OnAvgDone' are only loosely
 * coupled:
 *
 *     BSA_StoreData()
 *          -> stores data on a (input) queue
 *
 *                         BSA worker thread(s); works
 *                         on input queue computing averages;
 *                         output is stored in output queue.
 *                                  -> store avgs. in output queue
 *
 *                                       Dispatcher thread works on
 *                                       output queue(s) and dispatches
 *                                       Sink callbacks.
 */

struct BsaResultStruct {
	double         avg;
	double         rms;
	unsigned long  count;
	unsigned long  missed; // # of pulses with active EDEF but no data was received
	epicsTimeStamp timeStamp;
	BsaPulseId     pulseId;
	BsaStat        stat;
	BsaSevr        sevr;
};

typedef const struct BsaResultStruct *BsaResult;

/*
 * Release a result or an array of results; the
 * user may have their own queues to store results.
 * When done, the result must be released.
 *
 * Notes: Results are read-only and could be shared
 *        by multiple sinks.
 *
 *        An array of results is only released once
 *        (the individual members are not released)
 */
void
BSA_ReleaseResults(
	BsaResult results
);

struct BsaSimpleDataSinkStruct {
	/* called when a new BSA starts */
	void (*OnInit)(
		BsaChannel            self,
		const epicsTimeStamp *initTime,
		void                 *closure
	);

	/* called when one or more results are available
	 * to be consumed by this sink.
	 *
	 * Note: 'results' is an array of multiple
	 *       results then BSA_ReleaseResults()
	 *       must only be called once.
	 */
	void (*OnResult)(
		BsaChannel            self,
		BsaResult             results,
		unsigned              numResults,
		void                 *closure
	);


	/* called with error status (TBD) if a BSA is
	 * terminated.
	 */
	void (*OnAbort)(
		BsaChannel            self,
		const epicsTimeStamp *initTime,
		int                   status,
		void                 *closure
	);
};

typedef const struct BsaSimpleDataSinkStruct *BsaSimpleDataSink;

/*
 * Register a sink with a BSA channel for a given EDEF index.
 *
 * 'maxResults' specifies how many results may be delivered
 * to a single call of 'OnResult'.
 *
 * Returns: status (0 == OK)
 */
int
BSA_AddSimpleSink(
	BsaChannel        bsaChannel,
	BsaEdef           edefIndex,
	BsaSimpleDataSink sink,
	void             *closure,
	unsigned          maxResults
);

/*
 * Remove a sink
 *
 * Returns: status (0 == OK)
 */

int
BSA_DelSimpleSink(
	BsaChannel        bsaChannel,
	BsaEdef           edefIndex,
	BsaSimpleDataSink sink,
	void             *closure
);

/*
 * Obtain the timing callback function provided by BSA
 */
int
BSA_TimingCallbackRegister(int (*registrar)(BsaTimingCallback, void*));

/*
 * Notify BSA that the timing callbacks will no longer be called
 */
int
BSA_TimingCallbackUnregister();

/*
 * Dump statistics (stdout if a null arg is passed)
 *
 * If 'bsaChannel' is NULL then statistics for all
 * channels are printed.
 */
void
BSA_DumpChannelStats(BsaChannel bsaChannel, FILE *f);

void
BSA_DumpPatternBuffer(FILE *f);

#ifdef __cplusplus
}
#endif

#endif
