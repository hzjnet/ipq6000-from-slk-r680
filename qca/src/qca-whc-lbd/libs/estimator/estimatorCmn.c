// vim: set et sw=4 sts=4 cindent:
/*
 * @File: estimatorCmn.c
 *
 * @Abstract: Implementation of rate estimator API.
 *
 * @Notes:
 *
 * @@-COPYRIGHT-START-@@
 *
 * Copyright (c) 2015-2019 Qualcomm Technologies, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 * 2015-2016 Qualcomm Atheros, Inc.
 *
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
 * @@-COPYRIGHT-END-@@
 *
 */

#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <inttypes.h>

#ifdef LBD_DBG_MENU
#include <cmd.h>
#endif

#include "lb_assert.h"
#include "lb_common.h"
#include "module.h"
#include "profile.h"
#include "stadb.h"
#include "steeralg.h"
#include "steerexec.h"
#include "diaglog.h"

#include "estimator.h"
#include "estimatorCmn.h"
#include "estimatorRCPIToPhyRate.h"
#include "estimatorDiaglogDefs.h"
#include "estimatorSNRToPhyRateTable.h"
#include "estimatorCircularBuffer.h"
#include "estimatorPollutionAccumulator.h"
#include "estimatorInterferenceDetectionCurve.h"

#ifdef SON_MEMORY_DEBUG

#include "qca-son-mem-debug.h"
#undef QCA_MOD_INPUT
#define QCA_MOD_INPUT QCA_MOD_LBD_ESTIMATOR
#include "son-mem-debug.h"

#endif /* SON_MEMORY_DEBUG */

struct estimatorPriv_t estimatorState;

// Types used for temporary information during iteration.

/**
 * @brief Parameters that are needed for iterating over the supported
 *        BSSes when estimating the non-serving uplink RSSIs.
 */
typedef struct estimatorNonServingUplinkRSSIParams_t {
    /// The identity of the serving BSS.
    stadbEntry_bssStatsHandle_t  servingBSS;

    /// The identifying information for the serving BSS.
    const lbd_bssInfo_t *servingBSSInfo;

    /// The band on which the serving BSS operates.
    wlanif_band_e servingBand;

    /// The uplink RSSI on the serving BSS.
    lbd_rssi_t servingRSSI;

    /// The maximum transmit power on the BSS (from the AP's perspective).
    u_int8_t servingMaxTxPower;

    /// The result of the iterate (and thus the overall estimate).
    /// The callback will set this only on a failure.
    LBD_STATUS result;
} estimatorNonServingUplinkRSSIParams_t;

/**
 * @brief Parameters that are needed when iterating over the STAs to
 *        collect statistics.
 */
typedef struct estimatorSTAStatsSnapshotParams_t {
    /// The number of entries which still have stats pending.
    size_t numPending;
} estimatorSTAStatsSnapshotParams_t;

/**
 * @brief Parameters that are needed when iterating over the BSSes when
 *        writing back the estimated rates and airtime for other BSSes
 *        on the same band as the serving one on the same AP.
 */
typedef struct estimatorServingBandRateAirtimeParams_t {
    /// Whether to consider it a result overall or not.
    LBD_STATUS result;

    /// The band that is serving at the time of the measurement.
    wlanif_band_e servingBand;

    /// The last Tx rate value stored for the serving band.
    lbd_linkCapacity_t servingTxRate;

    /// The estimated SNR for serving BSS.
    lbd_snr_t servingSNR;

    /// Maximum Tx power on serving BSS
    u_int8_t servingTxPower;

    /// PHY capabilities supported by STA on serving band
    wlanif_phyCapInfo_t staPHY;

    /// The address of the STA for which the estimate is being done.
    const struct ether_addr *staAddr;
} estimatorServingBandRateAirtimeParams_t;

/**
 * @brief Parameters that are needed when iterating over the STAs to start
 *        the per-STA airtime estimate on a specific channel.
 */
typedef struct estimatorPerSTAAirtimeOnChannelParams_t {
    /// The AP to consider STAs from
    lbd_apId_t apId;

    /// The channel being estimated.
    lbd_channelId_t channelId;

    /// The freq being estimated.
    u_int16_t freq;

    /// The number of STAs for which an estimate was begun.
    size_t numSTAsStarted;
} estimatorPerSTAAirtimeOnChannelParams_t;

/**
 * @brief Parameters that are needed when iterating over the BSSes for a given
 *        STA to determine which to clear the pollution state for.
 */
typedef struct estimatorCmnPollutionCheckParams_t {
    /// The number of BSSes that are still marked as polluted.
    u_int8_t numPolluted;

    /// The number of previously polluted BSSes that have been cleared.
    u_int8_t numCleared;

    /// The next time a BSS will no longer be polluted.
    time_t minExpiryDeltaSecs;
} estimatorCmnPollutionCheckParams_t;

/**
 * @brief Parameters that are used in pollution state change callback function
 */
typedef struct estimatorCmnAccumulatorObserverCBParams_t {
    /// The handle to the STA
    stadbEntry_handle_t entry;

    /// MAC address of the STA
    const struct ether_addr *staAddr;

    /// The handle to the serving BSS of the STA
    stadbEntry_bssStatsHandle_t servingBSS;

    /// Information of the serving BSS
    const lbd_bssInfo_t *servingBSSInfo;

    /// Whether the STA was marked polluted on serving BSS before
    LBD_BOOL prevPolluted;
} estimatorCmnAccumulatorObserverCBParams_t;

/**
 * @brief Interference detection curve types
 */
typedef enum estimatorCmnInterferenceDetectionCurveType_e {
    /// 2.4 GHz, 20 MHz, 1 spacial stream
    estimatorCmnInterferenceDetectionCurveType_24G_20M_1SS,

    /// 2.4 GHz, 20 MHz, 2 spacial streams
    estimatorCmnInterferenceDetectionCurveType_24G_20M_2SS,

    /// 5 GHz, 40 MHz, 1 spacial stream
    estimatorCmnInterferenceDetectionCurveType_5G_40M_1SS,

    /// 5 GHz, 40 MHz, 2 spacial streams
    estimatorCmnInterferenceDetectionCurveType_5G_40M_2SS,

    /// 5 GHz, 80 MHz, 1 spacial stream
    estimatorCmnInterferenceDetectionCurveType_5G_80M_1SS,

    /// 5 GHz, 80 MHz, 2 spacial streams
    estimatorCmnInterferenceDetectionCurveType_5G_80M_2SS,

    /// Reserved value
    estimatorCmnInterferenceDetectionCurveType_invalid
} estimatorCmnInterferenceDetectionCurveType_e;

#define estimatorCmnCurveElementDefaultTableEntry(type, d0, rd1, md1, rd2, rmd1, md2) \
    { \
        { ESTIMATOR_CURVE_COEF_D0(type),                  #d0 }, \
        { ESTIMATOR_CURVE_COEF_RSSI_D1(type),            #rd1 }, \
        { ESTIMATOR_CURVE_COEF_MCS_D1(type),             #md1 }, \
        { ESTIMATOR_CURVE_COEF_RSSI_D2(type),            #rd2 }, \
        { ESTIMATOR_CURVE_COEF_RSSI_MCS_D1(type),       #rmd1 }, \
        { ESTIMATOR_CURVE_COEF_MCS_D2(type),             #md2 }, \
        { NULL, NULL} \
    }

/**
 * @brief Default configuration for interference detection curve
 */
static struct profileElement estimatorCmnCurveElementDefaultTable[][7] = {
    estimatorCmnCurveElementDefaultTableEntry(
        ESTIMATOR_IAS_CURVE_24G_20M_1SS,
        -1.742619, 0.083733, -0.068485, 0.000028, 0.000658, 0),
    estimatorCmnCurveElementDefaultTableEntry(
        ESTIMATOR_IAS_CURVE_24G_20M_2SS,
        -12.164426, 0.953627, -0.107352, -0.007219, -0.000042, 0),
    estimatorCmnCurveElementDefaultTableEntry(
        ESTIMATOR_IAS_CURVE_5G_40M_1SS,
        -7.267444, 0.501682, 0.005054, 0.001957, -0.003803, 0),
    estimatorCmnCurveElementDefaultTableEntry(
        ESTIMATOR_IAS_CURVE_5G_40M_2SS,
        -0.371811, -0.115817, -0.109735, 0.026610, -0.000762, 0),
    estimatorCmnCurveElementDefaultTableEntry(
        ESTIMATOR_IAS_CURVE_5G_80M_1SS,
        -7.682435, 0.706049, -0.013593, -0.006030, -0.000497, 0),
    estimatorCmnCurveElementDefaultTableEntry(
        ESTIMATOR_IAS_CURVE_5G_80M_2SS,
        -0.645476, -0.182679, -0.002149, 0.013230, -0.000587, 0),
};

/**
 * @brief Coefficients array for different capabilities
 */
estimatorInterferenceDetectionCurve_t
estimatorCmnInterferenceDetectionCurves[estimatorCmnInterferenceDetectionCurveType_invalid];

// Forward decls
static void estimatorCmnSTAFiniIterateCB(stadbEntry_handle_t entry,
                                         void *cookie);
static LBD_BOOL estimatorCmnNonServingUplinkRSSICallback(
    stadbEntry_handle_t entryHandle, stadbEntry_bssStatsHandle_t bssHandle,
    void *cookie);
static LBD_STATUS estimatorCmnStoreULRSSIEstimate(
        stadbEntry_handle_t entryHandle, wlanif_band_e servingBand,
        lbd_rssi_t servingRSSI, wlanif_band_e targetBand,
        int8_t powerDiff, stadbEntry_bssStatsHandle_t targetBSSStats);

static void estimatorCmnManageSTAStateLifecycleCB(stadbEntry_handle_t entry,
                                                  void *state);

static inline LBD_BOOL estimatorCmnStateIsSampling(const estimatorSTAState_t *state);

static inline LBD_BOOL estimatorCmnStateIs11kNotAllowed(const estimatorSTAState_t *state);

static LBD_STATUS estimatorCmnEstimateSTADataMetricsImpl(
        stadbEntry_handle_t handle, estimatorMeasurementMode_e measurementMode,
        steerexec_reason_e trigger);
static void estimatorCmnCompletePerSTAAirtime(estimatorSTAState_t *state,
                                              LBD_BOOL isFailure);

static LBD_STATUS estimatorCmnPerform11kMeasurement(stadbEntry_handle_t entry,
                                                    const struct ether_addr *addr,
                                                    estimatorSTAState_t *state);
static void estimatorCmnStart11kTimeout(estimatorSTAState_t *state,
                                        unsigned durationSecs,
                                        LBD_BOOL updateStateOnly);
static void estimatorCmnStart11kResponseTimeout(estimatorSTAState_t *state);
static void estimatorCmnStart11kProhibitTimeout(estimatorSTAState_t *state,
                                                LBD_STATUS dot11kResult,
                                                LBD_BOOL updateStateOnly,
                                                u_int8_t operatinClass);

static void estimatorCmnHandleBeaconReportEvent(struct mdEventNode *event);

static void estimatorCmn11kClientComplianceRcpiCheck(
                           struct wlanif_beaconReportEvent_t *bcnrptEvent,
                           stadbEntry_handle_t entry);

static void estimatorCmnNotifySTADataMetricsAllowedObservers(
        stadbEntry_handle_t entry);

static void estimatorCmnSTAStatsSampleTimeoutHandler(void *cookie);
static void estimatorCmn11kTimeoutHandler(void *cookie);
static void estimatorCmn11kReqTimeoutHandler(void *cookie);

static void estimatorCmnDiaglogNonServingStats(const struct ether_addr *addr,
                                               const lbd_bssInfo_t *bssInfo,
                                               lbd_linkCapacity_t capacity,
                                               lbd_airtime_t airtime);
static void estimatorCmnDiaglogSTAInterferenceDetected(
        const struct ether_addr *addr, const lbd_bssInfo_t *bssInfo,
        LBD_BOOL detected);
static void estimatorCmnDiaglogIASStats(
        const struct ether_addr *addr, const lbd_bssInfo_t *bssInfo,
        lbd_rssi_t rssi, lbd_linkCapacity_t txRate,
        u_int64_t byteCountDelta, u_int32_t packetCountDelta);

static void estimatorCmnStartSTAAirtimeIterateCB(stadbEntry_handle_t entry,
                                                 void *cookie);
static void estimatorCmnGeneratePerSTAAirtimeCompleteEvent(lbd_apId_t apId,
                                                           lbd_channelId_t channelId,
                                                           size_t numSTAsEstimated,
                                                           u_int16_t freq);

static void estimatorMenuInit(void);
static LBD_BOOL estimatorCmnCanSkipServingStats(
        const stadbEntry_handle_t handle,
        const stadbEntry_bssStatsHandle_t servingBSSStats,
        steerexec_reason_e trigger);
static LBD_BOOL estimatorCmnNonServingRateAirtimeCallback(
    stadbEntry_handle_t entryHandle, stadbEntry_bssStatsHandle_t bssHandle,
    void *cookie);

static LBD_STATUS estimatorCmnUpdatePollutionState(
        const struct ether_addr *staAddr, stadbEntry_handle_t entry,
        stadbEntry_bssStatsHandle_t servingBSS,
        const lbd_bssInfo_t *servingBSSInfo, LBD_BOOL polluted,
        LBD_BOOL prevPolluted);

static void estimatorCmnPollutionExpiryTimeoutHandler(void *cookie);
static void estimatorCmnCreatePollutionExpiryTimer(stadbEntry_handle_t handle,
                                                   estimatorSTAState_t *state);
static LBD_STATUS estimatorCmnAddInterferenceSample(stadbEntry_handle_t entry,
                                                    const struct ether_addr *staAddr,
                                                    stadbEntry_bssStatsHandle_t servingBSS,
                                                    const lbd_bssInfo_t *servingBSSInfo,
                                                    estimatorSTAState_t *state,
                                                    LBD_BOOL detected);
static LBD_STATUS estimatorCmnAccumulatorObserverCB(
        estimatorPollutionState_e pollutionState, void *cookie);
static void estimatorCmnHandleSTAStatsUpdate(
    const struct ether_addr *addr, const lbd_bssInfo_t *bss,
    const wlanif_staStats_t *stats, void *cookie);
static estimatorCmnInterferenceDetectionCurveType_e
estimatorCmnResolveInterferenceDetectionCurveTypeAndMaxRate(
        wlanif_band_e band, u_int8_t numSpatialStreams, const wlanif_phyCapInfo_t *phyCap,
        lbd_linkCapacity_t *maxRate);
static LBD_STATUS estimatorCmnInitInterferenceDetectionCurves(void);
static LBD_STATUS estimatorCmnHandleSTAStats(stadbEntry_handle_t entry,
                                             const struct ether_addr *staAddr,
                                             stadbEntry_bssStatsHandle_t servingBSS,
                                             const lbd_bssInfo_t *servingBSSInfo,
                                             const wlanif_staStats_t *staStats);

/**
 * @brief Default configuration values.
 *
 * These are used if the config file does not specify them.
 */
static struct profileElement estimatorElementDefaultTable[] = {
    { ESTIMATOR_AGE_LIMIT_KEY,                        "5" },
    { ESTIMATOR_RSSI_DIFF_EST_W5_FROM_W2_KEY,       "-15" },
    { ESTIMATOR_RSSI_DIFF_EST_W2_FROM_W5_KEY,         "5" },
    { ESTIMATOR_RSSI_DIFF_EST_W6_FROM_W2_KEY,       "-15" },
    { ESTIMATOR_RSSI_DIFF_EST_W2_FROM_W6_KEY,         "5" },
    { ESTIMATOR_RSSI_DIFF_EST_W6_FROM_W5_KEY,       "-15" },
    { ESTIMATOR_RSSI_DIFF_EST_W5_FROM_W6_KEY,         "5" },
    { ESTIMATOR_PROBE_COUNT_THRESHOLD_KEY,            "3" },
    { ESTIMATOR_STATS_SAMPLE_INTERVAL_KEY,            "1" },
    { ESTIMATOR_BACKHAUL_STA_STATS_SAMPLE_INTERVAL_KEY, "10" },
    { ESTIMATOR_MAX_11K_UNFRIENDLY_KEY,              "10" },
    { ESTIMATOR_11K_PROHIBIT_TIME_SHORT_KEY,         "30" },
    { ESTIMATOR_11K_PROHIBIT_TIME_LONG_KEY,         "300" },
    { ESTIMATOR_DELAYED_11K_REQUEST_TIMER,            "1" },
    { ESTIMATOR_LOW_PHY_RATE_THRESHOLD_KEY           "20" },
    { ESTIMATOR_HIGH_PHY_RATE_THRESHOLD_W2_KEY,     "200" },
    { ESTIMATOR_HIGH_PHY_RATE_THRESHOLD_W5_KEY,     "750" },
    { ESTIMATOR_HIGH_PHY_RATE_THRESHOLD_W6_KEY,     "750" },
    { ESTIMATOR_SCALING_FACTOR_LOW_KEY,              "60" },
    { ESTIMATOR_SCALING_FACTOR_MEDIUM_KEY,           "85" },
    { ESTIMATOR_SCALING_FACTOR_HIGH_KEY,             "60" },
    { ESTIMATOR_SCALING_FACTOR_TCP_KEY,              "90" },
    { ESTIMATOR_ENABLE_CONTINUOUS_THROUGHPUT_KEY ,    "0" },
    { ESTIMATOR_MAX_POLLUTION_TIME_KEY,            "1200" },
    { ESTIMATOR_FAST_POLLUTION_BUFSIZE_KEY,          "10" },
    { ESTIMATOR_NORMAL_POLLUTION_BUFSIZE_KEY,        "10" },
    { ESTIMATOR_POLLUTION_DETECT_THRESHOLD_KEY,      "60" },
    { ESTIMATOR_POLLUTION_CLEAR_THRESHOLD_KEY,       "40" },
    { ESTIMATOR_INTERFERENCE_AGE_LIMIT_KEY,          "15" },
    { ESTIMATOR_IAS_LOW_RSSI_THRESHOLD_KEY,          "12" },
    { ESTIMATOR_IAS_MAX_RATE_FACTOR_KEY,             "88" },
    { ESTIMATOR_IAS_MIN_DELTA_BYTES_KEY,           "2000" },
    { ESTIMATOR_IAS_MIN_DELTA_PACKETS_KEY,           "10" },
    { ESTIMATOR_IAS_ENABLE_SINGLE_BAND_DETECT_KEY,    "0" },
    { ESTIMATOR_ACT_DETECT_MIN_INTERVAL_KEY,         "10" },
    { ESTIMATOR_ACT_DETECT_MIN_PKT_PER_SEC_KEY,       "5" },
    { ESTIMATOR_INTERFERENCE_DETECTION_ENABLE_W2      "1" },
    { ESTIMATOR_INTERFERENCE_DETECTION_ENABLE_W5      "1" },
    { ESTIMATOR_INTERFERENCE_DETECTION_ENABLE_W6      "1" },
    { ESTIMATOR_MAX_STEERING_RETRY_COUNT_KEY,         "2" },
    { ESTIMATOR_RCPI_11K_COMPLIANT_DETECTION_THRESHOLD    "146" },
    { ESTIMATOR_RCPI_11K_NONCOMPLIANT_DETECTION_THRESHOLD "160" },
    { ESTIMATOR_RCPI_TYPE_CLASSIFICATION_ENABLE       "1" },
    { NULL, NULL }
};

#define USECS_PER_SEC 1000000

// The maximum value for a percentage (assuming percentages are represented
// as integers).
#define MAX_PERCENT 100

/// Minimum and maximum values for the config parameter that scales the
/// PHY rate before computing airtime.
#define MIN_PHY_RATE_SCALING 50
#define MAX_PHY_RATE_SCALING MAX_PERCENT

/// Maximum age of the serving data metrics to avoid doing another estimate.
/// This is not being made a config parameter for the time being to avoid
/// further proliferation of config options.
#define MAX_SERVING_METRICS_AGE_SECS 1

// ====================================================================
// Public API
// ====================================================================

LBD_STATUS estimator_init(void) {
    u_int8_t index = 0;
    int value[BSTEERING_MAX_CLIENT_CLASS_GROUP];

    estimatorState.dbgModule = dbgModuleFind("estimator");
    estimatorState.dbgModule->Level = DBGINFO;

    estimatorState.statsDbgModule = dbgModuleFind("ratestats");
    estimatorState.statsDbgModule->Level = DBGERR;

    estimatorState.airtimeOnChannelState.apId = LBD_APID_SELF;
    estimatorState.airtimeOnChannelState.channelId = LBD_CHANNEL_INVALID;
    estimatorState.airtimeOnChannelState.freq = LBD_FREQ_INVALID;

    estimatorState.config.ageLimit =
        profileGetOptsInt(mdModuleID_Estimator,
                          ESTIMATOR_AGE_LIMIT_KEY,
                          estimatorElementDefaultTable);
    if (profileGetOptsIntArray(mdModuleID_Estimator,
        ESTIMATOR_RSSI_DIFF_EST_W5_FROM_W2_KEY,
        estimatorElementDefaultTable, value) == LBD_NOK) {
        dbgf(estimatorState.dbgModule, DBGERR,
             "Unable to parse %s",
             ESTIMATOR_RSSI_DIFF_EST_W5_FROM_W2_KEY);
        return LBD_NOK;
    }
    for (index = 0; index < BSTEERING_MAX_CLIENT_CLASS_GROUP; index++) {
        estimatorState.config.rssiDiffEstimate5gFrom24g[index] = value[index];
    }

    if (profileGetOptsIntArray(mdModuleID_Estimator,
        ESTIMATOR_RSSI_DIFF_EST_W2_FROM_W5_KEY,
        estimatorElementDefaultTable, value) == LBD_NOK) {
        dbgf(estimatorState.dbgModule, DBGERR,
             "Unable to parse %s",
             ESTIMATOR_RSSI_DIFF_EST_W2_FROM_W5_KEY);
        return LBD_NOK;
    }
    for (index = 0; index < BSTEERING_MAX_CLIENT_CLASS_GROUP; index++) {
        estimatorState.config.rssiDiffEstimate24gFrom5g[index] = value[index];
    }

    if (profileGetOptsIntArray(mdModuleID_Estimator,
        ESTIMATOR_PROBE_COUNT_THRESHOLD_KEY,
        estimatorElementDefaultTable, value) == LBD_NOK) {
        dbgf(estimatorState.dbgModule, DBGERR,
             "Unable to parse %s",
             ESTIMATOR_PROBE_COUNT_THRESHOLD_KEY);
        return LBD_NOK;
    }
    for (index = 0; index < BSTEERING_MAX_CLIENT_CLASS_GROUP; index++) {
        estimatorState.config.probeCountThreshold[index] = value[index];
    }

    if (profileGetOptsIntArray(mdModuleID_Estimator,
        ESTIMATOR_RSSI_DIFF_EST_W6_FROM_W2_KEY,
        estimatorElementDefaultTable, value) == LBD_NOK) {
        dbgf(estimatorState.dbgModule, DBGERR,
             "Unable to parse %s",
             ESTIMATOR_RSSI_DIFF_EST_W6_FROM_W2_KEY);
        return LBD_NOK;
    }
    for (index = 0; index < BSTEERING_MAX_CLIENT_CLASS_GROUP; index++) {
        estimatorState.config.rssiDiffEstimate6gFrom24g[index] = value[index];
    }

    if (profileGetOptsIntArray(mdModuleID_Estimator,
        ESTIMATOR_RSSI_DIFF_EST_W2_FROM_W6_KEY,
        estimatorElementDefaultTable, value) == LBD_NOK) {
        dbgf(estimatorState.dbgModule, DBGERR,
             "Unable to parse %s",
             ESTIMATOR_RSSI_DIFF_EST_W2_FROM_W6_KEY);
        return LBD_NOK;
    }
    for (index = 0; index < BSTEERING_MAX_CLIENT_CLASS_GROUP; index++) {
        estimatorState.config.rssiDiffEstimate24gFrom6g[index] = value[index];
    }

    if (profileGetOptsIntArray(mdModuleID_Estimator,
        ESTIMATOR_RSSI_DIFF_EST_W6_FROM_W5_KEY,
        estimatorElementDefaultTable, value) == LBD_NOK) {
        dbgf(estimatorState.dbgModule, DBGERR,
             "Unable to parse %s",
             ESTIMATOR_RSSI_DIFF_EST_W6_FROM_W5_KEY);
        return LBD_NOK;
    }
    for (index = 0; index < BSTEERING_MAX_CLIENT_CLASS_GROUP; index++) {
        estimatorState.config.rssiDiffEstimate6gFrom5g[index] = value[index];
    }

    if (profileGetOptsIntArray(mdModuleID_Estimator,
        ESTIMATOR_RSSI_DIFF_EST_W5_FROM_W6_KEY,
        estimatorElementDefaultTable, value) == LBD_NOK) {
        dbgf(estimatorState.dbgModule, DBGERR,
             "Unable to parse %s",
             ESTIMATOR_RSSI_DIFF_EST_W5_FROM_W6_KEY);
        return LBD_NOK;
    }
    for (index = 0; index < BSTEERING_MAX_CLIENT_CLASS_GROUP; index++) {
        estimatorState.config.rssiDiffEstimate5gFrom6g[index] = value[index];
    }

    estimatorState.config.statsSampleInterval =
        profileGetOptsInt(mdModuleID_Estimator,
                          ESTIMATOR_STATS_SAMPLE_INTERVAL_KEY,
                          estimatorElementDefaultTable);
    estimatorState.config.backhaulStationStatsSampleInterval =
        profileGetOptsInt(mdModuleID_Estimator,
                          ESTIMATOR_BACKHAUL_STA_STATS_SAMPLE_INTERVAL_KEY,
                          estimatorElementDefaultTable);

    // The value is computed here so that the combined serving data metrics
    // and 802.11k measurement will be recent enough for steeralg to make a
    // decision.
    estimatorState.config.max11kResponseTime =
        estimatorState.config.ageLimit - MAX_SERVING_METRICS_AGE_SECS;

    estimatorState.config.max11kRequestTime = estimatorState.config.Delayed11kRequesttimer;

    estimatorState.config.max11kUnfriendly =
        profileGetOptsInt(mdModuleID_Estimator,
                          ESTIMATOR_MAX_11K_UNFRIENDLY_KEY,
                          estimatorElementDefaultTable);
    if (profileGetOptsIntArray(mdModuleID_Estimator,
        ESTIMATOR_11K_PROHIBIT_TIME_SHORT_KEY,
        estimatorElementDefaultTable, value) == LBD_NOK) {
        dbgf(estimatorState.dbgModule, DBGERR,
             "Unable to parse %s",
             ESTIMATOR_11K_PROHIBIT_TIME_SHORT_KEY);
        return LBD_NOK;
    }
    for (index = 0; index < BSTEERING_MAX_CLIENT_CLASS_GROUP; index++) {
        estimatorState.config.dot11kProhibitTimeShort[index] = value[index];
    }
    if (profileGetOptsIntArray(mdModuleID_Estimator,
        ESTIMATOR_11K_PROHIBIT_TIME_LONG_KEY,
        estimatorElementDefaultTable, value) == LBD_NOK) {
        dbgf(estimatorState.dbgModule, DBGERR,
             "Unable to parse %s",
             ESTIMATOR_11K_PROHIBIT_TIME_LONG_KEY);
        return LBD_NOK;
    }
    for (index = 0; index < BSTEERING_MAX_CLIENT_CLASS_GROUP; index++) {
        estimatorState.config.dot11kProhibitTimeLong[index] = value[index];
    }

    estimatorState.config.Delayed11kRequesttimer =
        profileGetOptsInt(mdModuleID_Estimator,
                          ESTIMATOR_DELAYED_11K_REQUEST_TIMER,
                          estimatorElementDefaultTable);
    estimatorState.config.lowPhyRateThreshold =
        profileGetOptsInt(mdModuleID_Estimator,
                          ESTIMATOR_LOW_PHY_RATE_THRESHOLD_KEY,
                          estimatorElementDefaultTable);
    estimatorState.config.highPhyRateThreshold_W2 =
        profileGetOptsInt(mdModuleID_Estimator,
                          ESTIMATOR_HIGH_PHY_RATE_THRESHOLD_W2_KEY,
                          estimatorElementDefaultTable);
    estimatorState.config.highPhyRateThreshold_W5 =
        profileGetOptsInt(mdModuleID_Estimator,
                          ESTIMATOR_HIGH_PHY_RATE_THRESHOLD_W5_KEY,
                          estimatorElementDefaultTable);
    estimatorState.config.highPhyRateThreshold_W6 =
        profileGetOptsInt(mdModuleID_Estimator,
                          ESTIMATOR_HIGH_PHY_RATE_THRESHOLD_W6_KEY,
                          estimatorElementDefaultTable);
    estimatorState.config.scalingFactorLow =
        profileGetOptsInt(mdModuleID_Estimator,
                          ESTIMATOR_SCALING_FACTOR_LOW_KEY,
                          estimatorElementDefaultTable);
    estimatorState.config.scalingFactorMedium =
        profileGetOptsInt(mdModuleID_Estimator,
                          ESTIMATOR_SCALING_FACTOR_MEDIUM_KEY,
                          estimatorElementDefaultTable);
    estimatorState.config.scalingFactorHigh =
        profileGetOptsInt(mdModuleID_Estimator,
                          ESTIMATOR_SCALING_FACTOR_HIGH_KEY,
                          estimatorElementDefaultTable);
    estimatorState.config.scalingFactorTCP =
        profileGetOptsInt(mdModuleID_Estimator,
                          ESTIMATOR_SCALING_FACTOR_TCP_KEY,
                          estimatorElementDefaultTable);

    estimatorState.config.enableContinuousThroughput =
        profileGetOptsInt(mdModuleID_Estimator,
                          ESTIMATOR_ENABLE_CONTINUOUS_THROUGHPUT_KEY,
                          estimatorElementDefaultTable);
    estimatorState.config.maxPollutionTime =
        profileGetOptsInt(mdModuleID_Estimator,
                          ESTIMATOR_MAX_POLLUTION_TIME_KEY,
                          estimatorElementDefaultTable);
    estimatorState.config.accumulator.fastPollutionDetectBufSize =
        profileGetOptsInt(mdModuleID_Estimator,
                          ESTIMATOR_FAST_POLLUTION_BUFSIZE_KEY,
                          estimatorElementDefaultTable);
    estimatorState.config.accumulator.normalPollutionDetectBufSize =
        profileGetOptsInt(mdModuleID_Estimator,
                          ESTIMATOR_NORMAL_POLLUTION_BUFSIZE_KEY,
                          estimatorElementDefaultTable);
    estimatorState.config.accumulator.pollutionDetectThreshold =
        profileGetOptsInt(mdModuleID_Estimator,
                          ESTIMATOR_POLLUTION_DETECT_THRESHOLD_KEY,
                          estimatorElementDefaultTable);
    estimatorState.config.accumulator.pollutionClearThreshold =
        profileGetOptsInt(mdModuleID_Estimator,
                          ESTIMATOR_POLLUTION_CLEAR_THRESHOLD_KEY,
                          estimatorElementDefaultTable);
    estimatorState.config.accumulator.interferenceAgeLimit =
        profileGetOptsInt(mdModuleID_Estimator,
                          ESTIMATOR_INTERFERENCE_AGE_LIMIT_KEY,
                          estimatorElementDefaultTable);
    estimatorState.config.iasLowRSSIThreshold =
        profileGetOptsInt(mdModuleID_Estimator,
                          ESTIMATOR_IAS_LOW_RSSI_THRESHOLD_KEY,
                          estimatorElementDefaultTable);
    estimatorState.config.iasMaxRateFactor =
        profileGetOptsInt(mdModuleID_Estimator,
                          ESTIMATOR_IAS_MAX_RATE_FACTOR_KEY,
                          estimatorElementDefaultTable);
    estimatorState.config.iasMinDeltaBytes =
        profileGetOptsInt(mdModuleID_Estimator,
                          ESTIMATOR_IAS_MIN_DELTA_BYTES_KEY,
                          estimatorElementDefaultTable);
    estimatorState.config.iasMinDeltaPackets =
        profileGetOptsInt(mdModuleID_Estimator,
                          ESTIMATOR_IAS_MIN_DELTA_PACKETS_KEY,
                          estimatorElementDefaultTable);
    estimatorState.config.iasEnableSingleBandDetect =
        profileGetOptsInt(mdModuleID_Estimator,
                          ESTIMATOR_IAS_ENABLE_SINGLE_BAND_DETECT_KEY,
                          estimatorElementDefaultTable);
    estimatorState.config.actDetectMinInterval =
        profileGetOptsInt(mdModuleID_Estimator,
                          ESTIMATOR_ACT_DETECT_MIN_INTERVAL_KEY,
                          estimatorElementDefaultTable);
    estimatorState.config.actDetectMinPktPerSec =
        profileGetOptsInt(mdModuleID_Estimator,
                          ESTIMATOR_ACT_DETECT_MIN_PKT_PER_SEC_KEY,
                          estimatorElementDefaultTable);
    estimatorState.config.backhaulActDetectMinPktPerSec =
        profileGetOptsInt(mdModuleID_Estimator,
                          ESTIMATOR_BACKHAUL_ACT_DETECT_MIN_PKT_PER_SEC_KEY,
                          estimatorElementDefaultTable);

    estimatorState.config.interferenceDetectionEnable[wlanif_band_24g] =
        profileGetOptsInt(mdModuleID_Estimator,
                          ESTIMATOR_INTERFERENCE_DETECTION_ENABLE_W2,
                          estimatorElementDefaultTable);
    estimatorState.config.interferenceDetectionEnable[wlanif_band_5g] =
        profileGetOptsInt(mdModuleID_Estimator,
                          ESTIMATOR_INTERFERENCE_DETECTION_ENABLE_W5,
                          estimatorElementDefaultTable);
    estimatorState.config.interferenceDetectionEnable[wlanif_band_6g] =
        profileGetOptsInt(mdModuleID_Estimator,
                          ESTIMATOR_INTERFERENCE_DETECTION_ENABLE_W6,
                          estimatorElementDefaultTable);

    estimatorState.config.apSteerMaxRetryCount =
        profileGetOptsInt(mdModuleID_Estimator,
                          ESTIMATOR_MAX_STEERING_RETRY_COUNT_KEY,
                          estimatorElementDefaultTable);

    estimatorState.config.rcpi11kCompliantDetectionThreshold =
        profileGetOptsInt(mdModuleID_Estimator,
                          ESTIMATOR_RCPI_11K_COMPLIANT_DETECTION_THRESHOLD,
                          estimatorElementDefaultTable);

    estimatorState.config.rcpi11kNonCompliantDetectionThreshold =
        profileGetOptsInt(mdModuleID_Estimator,
                          ESTIMATOR_RCPI_11K_NONCOMPLIANT_DETECTION_THRESHOLD,
                          estimatorElementDefaultTable);

    estimatorState.config.enableRcpiTypeClassification =
        profileGetOptsInt(mdModuleID_Estimator,
                          ESTIMATOR_RCPI_TYPE_CLASSIFICATION_ENABLE,
                          estimatorElementDefaultTable);

    // Sanity check the values
    for (index = 0; index < BSTEERING_MAX_CLIENT_CLASS_GROUP; index++) {
        if (estimatorState.config.max11kResponseTime >
                estimatorState.config.dot11kProhibitTimeShort[index]) {
            dbgf(estimatorState.dbgModule, DBGERR,
                 "%s: 802.11k response timeout must be smaller than "
                 "802.11k prohibit timeout", __func__);
            return LBD_NOK;
        }
        if (estimatorState.config.dot11kProhibitTimeShort[index] >
                estimatorState.config.dot11kProhibitTimeLong[index]) {
            dbgf(estimatorState.dbgModule, DBGERR,
                 "%s: 802.11k short prohibit timeout cannot be larger than "
                 "802.11k long prohibit timeout", __func__);
            return LBD_NOK;
        }
    }

    if (!estimatorPollutionAccumulatorAreValidParams(estimatorState.dbgModule,
                                                     &estimatorState.config.accumulator)) {
        return LBD_NOK;
    }

    if (LBD_NOK == estimatorCmnInitInterferenceDetectionCurves()) {
        return LBD_NOK;
    }

    evloopTimeoutCreate(&estimatorState.statsSampleTimer,
                        "estimatorSTAStatsSampleTimeout",
                        estimatorCmnSTAStatsSampleTimeoutHandler,
                        NULL);

    evloopTimeoutCreate(&estimatorState.dot11kTimer,
                        "estimator11kTimeout",
                        estimatorCmn11kTimeoutHandler,
                        NULL);

    evloopTimeoutCreate(&estimatorState.dot11kReqTimer,
                        "estimator11kTimeout",
                        estimatorCmn11kReqTimeoutHandler,
                        NULL);

    mdEventTableRegister(mdModuleID_Estimator, estimator_event_maxnum);

    mdListenTableRegister(mdModuleID_WlanIF, wlanif_event_beacon_report,
                          estimatorCmnHandleBeaconReportEvent);

    estimatorMenuInit();

    if (estimatorState.config.enableContinuousThroughput) {
        evloopTimeoutRegister(&estimatorState.statsSampleTimer,
                              estimatorState.config.statsSampleInterval,
                              0 /* usec */);
    }

    // Register for wlanif STA stats events
    if (wlanif_registerSTAStatsObserver(estimatorCmnHandleSTAStatsUpdate,
                                        NULL) == LBD_NOK) {
        return LBD_NOK;
    }

    estimatorSubInit();

    return LBD_OK;
}

LBD_STATUS estimator_estimateNonServingUplinkRSSI(stadbEntry_handle_t handle) {
    estimatorNonServingUplinkRSSIParams_t params;

    if (!estimatorCmnIsSelfServing(handle, &params.servingBSS, &params.servingBSSInfo) &&
        !estimatorIsRemoteEstimationAllowed()) {
        dbgf(estimatorState.dbgModule, DBGERR,
             "%s: Called with a STA not being served by this AP", __func__);
        return LBD_NOK;
    }

    params.servingBand =
        wlanif_resolveBandFromFreq(params.servingBSSInfo->freq);

    wlanif_phyCapInfo_t servingPhyCap = { LBD_FALSE /* valid*/};
    if (LBD_NOK == wlanif_getBSSPHYCapInfo(params.servingBSSInfo, &servingPhyCap) ||
        !servingPhyCap.valid) {
        dbgf(estimatorState.dbgModule, DBGERR,
             "%s: Unable to resolve the serving BSS PHY capabilities for "
             lbBSSInfoAddFmt(), __func__,
             lbBSSInfoAddData(params.servingBSSInfo));
        return LBD_NOK;
    }

    params.servingMaxTxPower = servingPhyCap.maxTxPower;

    // Note here that we do not care about the age or number of probes, as
    // it is a precondition of this function call that the serving RSSI
    // is up-to-date.
    params.servingRSSI = stadbEntry_getUplinkRSSI(handle, params.servingBSS,
                                                  NULL, NULL);
    if (LBD_INVALID_RSSI == params.servingRSSI) {
        dbgf(estimatorState.dbgModule, DBGERR,
             "%s: Unable to resolve the serving uplink RSSI",
             __func__);
        return LBD_NOK;
    }

    // Iterate to fill in any of the non-serving RSSIs.
    params.result = LBD_OK;
    if (stadbEntry_iterateBSSStats(handle,
                                   estimatorCmnNonServingUplinkRSSICallback,
                                   &params, NULL, NULL) != LBD_OK) {
        dbgf(estimatorState.dbgModule, DBGERR,
             "%s: Failed to iterate over non-serving BSS stats",
             __func__);
        return LBD_NOK;
    }

    return params.result;
}

LBD_BOOL estimator_is11kUnfriendly(stadbEntry_handle_t handle) {
    estimatorSTAState_t *state =
        (estimatorSTAState_t *) stadbEntry_getEstimatorState(handle);
    if (state) {
        return (state->countConsecutive11kFailure >= estimatorState.config.max11kUnfriendly) ?
                LBD_TRUE : LBD_FALSE;
    } else {
        return LBD_FALSE;
    }
}

LBD_STATUS estimator_estimateSTADataMetrics(stadbEntry_handle_t handle,
                                            steerexec_reason_e trigger) {
    estimatorMeasurementMode_e mode = estimatorMeasurementMode_full;
    if (trigger == steerexec_reason_activeLegacyAPSteering) {
        mode = estimatorMeasurementMode_staStatsOnly;
    }

    return estimatorCmnEstimateSTADataMetricsImpl(handle, mode, trigger);
}

LBD_STATUS estimator_estimatePerSTAAirtimeOnChannel(lbd_apId_t apId, 
                                                    lbd_channelId_t channelId,
                                                    u_int16_t freq ) {
    if (LBD_CHANNEL_INVALID == channelId) {
        return LBD_NOK;
    }

    if (estimatorState.airtimeOnChannelState.channelId != LBD_CHANNEL_INVALID) {
        dbgf(estimatorState.dbgModule, DBGERR,
             "%s: Airtime measurement already in progress on channel [%u]; "
             "cannot service request for channel [%u]", __func__,
             estimatorState.airtimeOnChannelState.channelId, channelId);
        return LBD_NOK;
    }

    dbgf(estimatorState.dbgModule, DBGINFO,
         "%s: Estimating per-STA airtime for AP [%u] on channel [%u]",
         __func__, apId, channelId);

    estimatorPerSTAAirtimeOnChannelParams_t params;
    params.channelId = channelId;
    params.apId = apId;
    params.numSTAsStarted = 0;
    params.freq = freq;
    if (stadb_iterate(estimatorCmnStartSTAAirtimeIterateCB, &params) != LBD_OK) {
        dbgf(estimatorState.dbgModule, DBGERR,
             "%s: Failed to iterate over STA DB; no estimates will be done",
             __func__);
        return LBD_NOK;
    }

    if (!params.numSTAsStarted) {
        estimatorCmnGeneratePerSTAAirtimeCompleteEvent(apId, channelId,
                                                       0 /* numSTAsEstimated */,
                                                       freq);
    } else {
        estimatorState.airtimeOnChannelState.apId = apId;
        estimatorState.airtimeOnChannelState.channelId = channelId;
        estimatorState.airtimeOnChannelState.freq = freq;
        estimatorState.airtimeOnChannelState.numSTAsRemaining =
            params.numSTAsStarted;
        estimatorState.airtimeOnChannelState.numSTAsSuccess = 0;
        estimatorState.airtimeOnChannelState.numSTAsFailure = 0;
    }

    return LBD_OK;
}

lbd_linkCapacity_t estimator_estimateTCPFullCapacity(wlanif_band_e band,
                                                     lbd_linkCapacity_t phyRate) {
    u_int32_t scaleFactor = estimatorState.config.scalingFactorMedium;
    if (phyRate < estimatorState.config.lowPhyRateThreshold) {
        scaleFactor = estimatorState.config.scalingFactorLow;
    } else if ((band == wlanif_band_24g &&
                phyRate >= estimatorState.config.highPhyRateThreshold_W2) ||
               (band == wlanif_band_5g &&
                phyRate >= estimatorState.config.highPhyRateThreshold_W5) ||
               (band == wlanif_band_6g &&
                phyRate >= estimatorState.config.highPhyRateThreshold_W6)) {
        scaleFactor = estimatorState.config.scalingFactorHigh;
    }

    lbd_linkCapacity_t tcpRate =
        (phyRate * scaleFactor * estimatorState.config.scalingFactorTCP) /
        (MAX_PERCENT * MAX_PERCENT);
    return tcpRate;
}

LBD_STATUS estimator_registerSTADataMetricsAllowedObserver(
        estimator_staDataMetricsAllowedObserverCB callback, void *cookie) {
    if (!callback) {
        return LBD_NOK;
    }

    struct estimatorSTADataMetricsAllowedObserver *freeSlot = NULL;
    size_t i;
    for (i = 0; i < MAX_STA_DATA_METRICS_ALLOWED_OBSERVERS; ++i) {
        struct estimatorSTADataMetricsAllowedObserver *curSlot =
            &estimatorState.staDataMetricsAllowedObservers[i];
        if (curSlot->isValid && curSlot->callback == callback &&
            curSlot->cookie == cookie) {
            dbgf(estimatorState.dbgModule, DBGERR,
                 "%s: Duplicate registration (func %p, cookie %p)",
                 __func__, callback, cookie);
           return LBD_NOK;
        }

        if (!freeSlot && !curSlot->isValid) {
            freeSlot = curSlot;
        }

    }

    if (freeSlot) {
        freeSlot->isValid = LBD_TRUE;
        freeSlot->callback = callback;
        freeSlot->cookie = cookie;
        return LBD_OK;
    }

    // No free entries found.
    return LBD_NOK;
}

LBD_STATUS estimator_unregisterSTADataMetricsAllowedObserver(
        estimator_staDataMetricsAllowedObserverCB callback, void *cookie) {
    if (!callback) {
        return LBD_NOK;
    }

    size_t i;
    for (i = 0; i < MAX_STA_DATA_METRICS_ALLOWED_OBSERVERS; ++i) {
        struct estimatorSTADataMetricsAllowedObserver *curSlot =
            &estimatorState.staDataMetricsAllowedObservers[i];
        if (curSlot->isValid && curSlot->callback == callback &&
            curSlot->cookie == cookie) {
            curSlot->isValid = LBD_FALSE;
            curSlot->callback = NULL;
            curSlot->cookie = NULL;
            return LBD_OK;
        }
    }

    // No match found
    return LBD_NOK;
}

LBD_STATUS estimator_fini(void) {
    estimatorSubFini();

    LBD_STATUS status = wlanif_unregisterSTAStatsObserver(
        estimatorCmnHandleSTAStatsUpdate, NULL);

    // Need to disable the stats for any entries that are still in a sampling
    // mode.
    if (stadb_iterate(estimatorCmnSTAFiniIterateCB, NULL) == LBD_NOK) {
        status = LBD_NOK;
    }

    return status;
}

// ====================================================================
// Package level functions
// ====================================================================

LBD_STATUS estimatorCmnResolveMinPhyCap(
        stadbEntry_handle_t entry, const struct ether_addr *addr,
        stadbEntry_bssStatsHandle_t bssStats, const lbd_bssInfo_t *bssInfo,
        const wlanif_phyCapInfo_t *bssCap, wlanif_phyCapInfo_t *minPhyCap) {
    wlanif_phyCapInfo_t staCap = { LBD_FALSE };
    if (stadbEntry_getPHYCapInfo(entry, bssStats, &staCap) || !staCap.valid) {
        dbgf(estimatorState.dbgModule, DBGERR,
             "%s: Failed to resolve STA capabilities for " lbMACAddFmt(":")
             " on " lbBSSInfoAddFmt(),
             __func__, lbMACAddData(addr->ether_addr_octet),
             lbBSSInfoAddData(bssInfo));
        return LBD_NOK;
    }

    wlanif_resolveMinPhyCap(bssCap, &staCap, minPhyCap);

    return LBD_OK;
}

LBD_STATUS estimatorCmnEstimateNonServingRateAirtime(
    stadbEntry_handle_t entry, const struct ether_addr *staAddr, u_int8_t flags,
    stadbEntry_bssStatsHandle_t measuredBSSStats,
    stadbEntry_bssStatsHandle_t nonServingBSSStats, const lbd_bssInfo_t *targetBSSInfo,
    const wlanif_phyCapInfo_t *targetPHYCap, lbd_rcpi_t measuredRCPI,
    u_int8_t measuredBSSTxPower) {
    lbd_rcpi_t rcpi = LBD_INVALID_RCPI;
    lbd_linkCapacity_t capacity =
        estimatorEstimateFullCapacityFromRCPI(
                estimatorState.dbgModule, entry, targetBSSInfo, targetPHYCap,
                measuredBSSStats, measuredRCPI, measuredBSSTxPower, &rcpi);

    // Even with an invalid capacity, still proceed to store the value to
    // ensure other modules don't use a stale value.

    // Compute the airtime and store it in the entry.
    if (LBD_NOK == estimatorCmnComputeAndStoreStats(entry, staAddr, flags,
                                                    nonServingBSSStats, capacity, rcpi)) {
        dbgf(estimatorState.dbgModule, DBGERR,
             "%s: Failed to write back capacity and airtime for "
             lbMACAddFmt(":") " on " lbBSSInfoAddFmt(),
             __func__,
             lbMACAddData(staAddr->ether_addr_octet),
             lbBSSInfoAddData(targetBSSInfo));
        return LBD_NOK;
    }

    // Only return success when we had a valid link capacity estimate
    return (LBD_INVALID_LINK_CAP == capacity) ? LBD_NOK : LBD_OK;
}

void estimatorCmnHandleLocalBeaconReport(stadbEntry_handle_t entry,
                                         const struct ether_addr *staAddr,
                                         const lbd_bssInfo_t *reportedLocalBss,
                                         estimatorNonServingRateAirtimeParams_t *params) {
    params->measuredBSSStats = stadbEntry_findMatchBSSStats(entry, reportedLocalBss);
    if (!params->measuredBSSStats) {
        dbgf(estimatorState.dbgModule, DBGERR,
             "%s: Failed to find matching stats for " lbMACAddFmt(":")
             " on " lbBSSInfoAddFmt(), __func__,
             lbMACAddData(staAddr->ether_addr_octet),
             lbBSSInfoAddData(reportedLocalBss));
        params->result = LBD_NOK;
        return;
    }

    wlanif_phyCapInfo_t phyCap = { LBD_FALSE /* valid */};
    if (LBD_OK == wlanif_getBSSPHYCapInfo(reportedLocalBss, &phyCap) &&
        phyCap.valid) {
        params->txPower = phyCap.maxTxPower;
    } else {
        // Even though unable to get PHY capability on the reported BSS, we still
        // want to try other BSSes on the same band, since we can only do 11k measurement
        // every 30 seconds by default.
        dbgf(estimatorState.dbgModule, DBGERR,
             "%s: Failed to resolve PHY capability on measured BSS ("
             lbBSSInfoAddFmt() "), will assume no Tx power difference "
             "when estimating rates on other same-band BSSes",
             __func__, lbBSSInfoAddData(reportedLocalBss));
        params->txPower = 0;
    }

    if (stadbEntry_iterateBSSStats(entry,
                                   estimatorCmnNonServingRateAirtimeCallback,
                                   params, NULL, NULL) != LBD_OK) {
        dbgf(estimatorState.dbgModule, DBGERR,
             "%s: Failed to iterate over non-serving BSS stats",
             __func__);
        params->result = LBD_NOK;
    }
}

void estimatorCmnDiaglogSTAPollutionChanged(
        const struct ether_addr *addr, const lbd_bssInfo_t *bssInfo,
        LBD_BOOL polluted, estimatorPollutionChangedReason_e reasonCode) {
    if (diaglog_startEntry(mdModuleID_Estimator,
                           estimator_msgId_staPollutionChanged,
                           diaglog_level_demo)) {
        diaglog_writeMAC(addr);
        diaglog_writeBSSInfo(bssInfo);
        diaglog_write8(polluted);
        diaglog_write8(reasonCode);
        diaglog_finishEntry();
    }
}

void estimatorCmnStartPollutionTimer(
        const struct ether_addr *staAddr, stadbEntry_handle_t entry,
        unsigned timerSecs) {
    estimatorSTAState_t *state = estimatorCmnGetOrCreateSTAState(entry);
    if (!state) {
        dbgf(estimatorState.dbgModule, DBGERR,
             "%s: Failed to create state for " lbMACAddFmt(":"),
             __func__, lbMACAddData(staAddr->ether_addr_octet));
        return;
    }

    unsigned remainingSecs, remainingUsecs;
    evloopTimeoutRemaining(&state->pollutionExpiryTimer, &remainingSecs,
                           &remainingUsecs);

    if (!remainingSecs || timerSecs < remainingSecs) {
        evloopTimeoutRegister(&state->pollutionExpiryTimer, timerSecs,
                              0 /* usecs */);
    }
}

void estimatorCmnGeneratePollutionClearEvent(const struct ether_addr *staAddr) {
    estimator_staPollutionClearedEvent_t event;
    lbCopyMACAddr(staAddr->ether_addr_octet, event.addr.ether_addr_octet);
    mdCreateEvent(mdModuleID_Estimator, mdEventPriority_Low,
                  estimator_event_staPollutionCleared, &event, sizeof(event));
}

void estimatorCmnCompleteSTAStatsSample(stadbEntry_handle_t entry,
                                        const struct ether_addr *addr,
                                        estimatorSTAState_t *state,
                                        const struct timespec *sampleTime,
                                        const wlanif_staStatsSnapshot_t *stats,
                                        LBD_BOOL isFailure) {
    LBD_BOOL skipDisable = LBD_FALSE;

    // Always assume we are done. This will be adjusted in the continuous
    // throughput mode below.
    state->throughputState = estimatorThroughputState_idle;
    if (isFailure) {
        dbgf(estimatorState.dbgModule, DBGERR,
             "%s: Aborting STA stats measurement for " lbMACAddFmt(":")
             " in state %u", __func__, lbMACAddData(addr->ether_addr_octet),
             state->throughputState);
    } else if (state->measurementMode == estimatorMeasurementMode_full) {
        estimatorCmnPerform11kMeasurement(entry, addr, state);
    } else if (state->measurementMode == estimatorMeasurementMode_staStatsOnly) {
        estimator_staTputEstCompleteEvent_t event;
        lbCopyMACAddr(addr->ether_addr_octet, event.staAddr.ether_addr_octet);
        mdCreateEvent(mdModuleID_Estimator, mdEventPriority_Low,
                      estimator_event_staTputEstComplete, &event, sizeof(event));
    }

    stadbEntry_bssStatsHandle_t servingBSS;
    const lbd_bssInfo_t *servingBSSInfo;
    LBD_BOOL selfServing = estimatorCmnIsSelfServing(entry, &servingBSS, &servingBSSInfo);

    if (!isFailure && (estimatorState.config.enableContinuousThroughput ||
                       (servingBSS && !selfServing) ||
                       (servingBSS && stadbEntry_isBackhaulSTA(entry)))) {
        // Store the last snapshot as the first sample and stay in the
        // state awaiting the second sample.
        lbDbgAssertExit(estimatorState.dbgModule, sampleTime);
        lbDbgAssertExit(estimatorState.dbgModule, stats);

        state->lastSampleTime = *sampleTime;
        state->lastStatsSnapshot = *stats;
        state->throughputState = estimatorThroughputState_awaitingSecondSample;
        skipDisable = LBD_TRUE;
    }

    if (!skipDisable) {
        dbgf(estimatorState.dbgModule, DBGDEBUG,
             "%s: Disabling stats on " lbBSSInfoAddFmt(), __func__,
             lbBSSInfoAddData(&state->statsEnabledBSSInfo));
        wlanif_disableSTAStats(&state->statsEnabledBSSInfo);
    }

    if (state->measurementMode != estimatorMeasurementMode_staStatsOnly) {
        estimatorCmnCompletePerSTAAirtime(state, isFailure);
    }

    // Set remotely associated STAs to throughputOnly
    if (!isFailure && (estimatorState.config.enableContinuousThroughput ||
        (servingBSS && !selfServing))) {
        state->measurementMode = selfServing ? estimatorMeasurementMode_throughputOnly
                                             : estimatorMeasurementMode_remoteThroughput;
    } else {
        state->measurementMode = estimatorMeasurementMode_none;
    }
}

// ====================================================================
// Private helper functions
// ====================================================================

/**
 * @brief Disable the statistics collection for any STAs where it was started.
 */
static void estimatorCmnSTAFiniIterateCB(stadbEntry_handle_t entry,
                                         void *cookie) {
    estimatorSTAState_t *state = stadbEntry_getEstimatorState(entry);
    if (state && state->measurementMode != estimatorMeasurementMode_none) {
        dbgf(estimatorState.dbgModule, DBGDEBUG,
             "%s: Disabling stats on " lbBSSInfoAddFmt(), __func__,
             lbBSSInfoAddData(&state->statsEnabledBSSInfo));
        wlanif_disableSTAStats(&state->statsEnabledBSSInfo);
    }
}

/**
 * @brief Check the RSSI for a given non-serving BSS and estimate it if
 *        it does not meet the recency/accuracy requirements.
 *
 * @param [in] entryHandle  the STA being processed
 * @param [in] bssHandle  the BSS for which to update the RSSI (if necesasry)
 * @param [in] cookie  the internal parameters for the iteration
 *
 * @return LBD_FALSE always (as it does not keep the BSSes around)
 */
static LBD_BOOL estimatorCmnNonServingUplinkRSSICallback(
    stadbEntry_handle_t entryHandle, stadbEntry_bssStatsHandle_t bssHandle,
    void *cookie) {
    estimatorNonServingUplinkRSSIParams_t *params =
        (estimatorNonServingUplinkRSSIParams_t *) cookie;
    lbDbgAssertExit(estimatorState.dbgModule, params);

    if (params->servingBSS != bssHandle) {
        const lbd_bssInfo_t *targetBSSInfo =
            stadbEntry_resolveBSSInfo(bssHandle);
        lbDbgAssertExit(estimatorState.dbgModule, targetBSSInfo);

        // Must be from the same AP for us to be able to estimate.
        if (lbAreBSSInfoSameAP(params->servingBSSInfo, targetBSSInfo)) {
            time_t rssiAgeSecs;
            u_int8_t probeCount;
            u_int8_t clientClassGroup = 0;
            lbd_rssi_t rssi = stadbEntry_getUplinkRSSI(entryHandle, bssHandle,
                                                       &rssiAgeSecs, &probeCount);
            stadbEntry_getClientClassGroup(entryHandle, &clientClassGroup);
            if (LBD_INVALID_RSSI == rssi ||
                rssiAgeSecs > estimatorState.config.ageLimit ||
                (probeCount > 0 &&
                 probeCount < estimatorState.config.probeCountThreshold[clientClassGroup])) {
                wlanif_phyCapInfo_t nonServingPhyCap = { LBD_FALSE /* valid */};
                if (LBD_OK ==  wlanif_getBSSPHYCapInfo(targetBSSInfo, &nonServingPhyCap) &&
                    nonServingPhyCap.valid) {
                    int8_t powerDiff =
                        nonServingPhyCap.maxTxPower - params->servingMaxTxPower;
                    wlanif_band_e targetBand =
                        wlanif_resolveBandFromFreq(targetBSSInfo->freq);
                    if (estimatorCmnStoreULRSSIEstimate(entryHandle, params->servingBand,
                                                        params->servingRSSI,
                                                        targetBand, powerDiff,
                                                        bssHandle) != LBD_OK) {
                        dbgf(estimatorState.dbgModule, DBGERR,
                             "%s: Failed to store estimate for BSS: " lbBSSInfoAddFmt(),
                             __func__, lbBSSInfoAddData(targetBSSInfo));

                        // Store the failure so the overall result can be failed.
                        params->result = LBD_NOK;
                    }
                } else {
                    dbgf(estimatorState.dbgModule, DBGERR,
                         "%s: Unable to resolve the non-serving BSS PHY capabilities for "
                         lbBSSInfoAddFmt(), __func__, lbBSSInfoAddData(targetBSSInfo));
                    params->result = LBD_NOK;
                }
            }
        }
    }

    return LBD_FALSE;
}

/**
 * @brief Compupte the RSSI estimate for the specified BSS and store it.
 *
 * @param [in] entryHandle  the STA being processed
 * @param [in] servingBand  the handle to the serving BSS
 * @param [in] servingRSSI  the RSSI on the serving BSS
 * @param [in] targetBand  the target BSS's band
 * @param [in] powerDiff  the difference in Tx power between the serving and
 *                        non-serving BSSes
 * @param [in] targetBSS  the non-serving BSS to update
 *
 * @return LBD_OK if the estimate was stored successfully; otherwise LBD_NOK
 */
static LBD_STATUS estimatorCmnStoreULRSSIEstimate(
        stadbEntry_handle_t entryHandle, wlanif_band_e servingBand,
        lbd_rssi_t servingRSSI, wlanif_band_e targetBand,
        int8_t powerDiff, stadbEntry_bssStatsHandle_t targetBSSStats) {
    // Since clients may be more limited in transmission power, we act
    // conservatively and say that any cases where the target BSS has
    // higher power than the source does not necessarily translate into
    // an improved uplink RSSI.
    if (powerDiff > 0) {
        powerDiff = 0;
    }

    // For now, if they are on the same band, we assume the RSSIs are
    // equal. In the future we can potentially consider any Tx power
    // limitations of the client (although clients do not always provide
    // meaningful values so this is likely not usable).
    int8_t deltaRSSI = powerDiff;

    u_int8_t clientClassGroup = 0;
    if (LBD_NOK == stadbEntry_getClientClassGroup(entryHandle, &clientClassGroup)) {
        dbgf(estimatorState.dbgModule, DBGERR,
                         "%s: Failed to resolve operating class", __func__);
        return LBD_NOK;
    }

    if (servingBand != targetBand) {
        switch (targetBand) {
            case wlanif_band_24g:
                if ( stadbEntry_isBandSupported(entryHandle, wlanif_band_6g) ) {
                    deltaRSSI += estimatorState.config.rssiDiffEstimate24gFrom6g[clientClassGroup];
                } else {
                    deltaRSSI += estimatorState.config.rssiDiffEstimate24gFrom5g[clientClassGroup];
                }
                break;

            case wlanif_band_5g:
                if ( stadbEntry_isBandSupported(entryHandle, wlanif_band_6g) ) {
                    deltaRSSI += estimatorState.config.rssiDiffEstimate5gFrom6g[clientClassGroup];
                }
                else {
                    deltaRSSI += estimatorState.config.rssiDiffEstimate5gFrom24g[clientClassGroup];
                }
                break;

            case wlanif_band_6g:
                if ( stadbEntry_isBandSupported(entryHandle, wlanif_band_5g) ) {
                    deltaRSSI += estimatorState.config.rssiDiffEstimate6gFrom5g[clientClassGroup];
                } else {
                    deltaRSSI += estimatorState.config.rssiDiffEstimate6gFrom24g[clientClassGroup];
                }
                break;

            default:
                // Somehow failed to resolve target band.
                dbgf(estimatorState.dbgModule, DBGERR,
                     "%s: Failed to resolve target band for BSS %p",
                     __func__, targetBSSStats);
                return LBD_NOK;
        }
    }

    // Need to make sure we did not underflow (and thus end up with a large
    // positive number). If so, we force the RSSI to 0 to indicate we likely
    // would be unable to associate on 5 GHz.
    //
    // We are assuming that we will never overflow, as the maximum serving
    // RSSI value plus the adjustment will be much less than the size of an
    // 8-bit integer.
    lbd_rssi_t targetRSSI;
    if (deltaRSSI < 0 && (-deltaRSSI > servingRSSI)) {
        targetRSSI = 0;
    } else {
        targetRSSI = servingRSSI + deltaRSSI;
    }

    return stadbEntry_setUplinkRSSI(entryHandle, targetBSSStats, targetRSSI,
                                    LBD_TRUE /* estimated */);
}

/**
 * @brief Obtain the estimator state for the STA, creating it if it does not
 *        exist.
 *
 * @param [in] entry  the handle to the STA for which to set the state
 *
 * @return the state entry, or NULL if one could not be created
 */
estimatorSTAState_t *estimatorCmnGetOrCreateSTAState(
        stadbEntry_handle_t entry) {
    estimatorSTAState_t *state =
        (estimatorSTAState_t *) stadbEntry_getEstimatorState(entry);
    if (!state) {
        state = (estimatorSTAState_t *) calloc(1, sizeof(estimatorSTAState_t));
        if (!state) {
            return NULL;
        }

        state->pollutionAccumulator = estimatorCircularBufferCreate(
                estimatorState.config.accumulator.fastPollutionDetectBufSize,
                estimatorState.config.accumulator.normalPollutionDetectBufSize,
                estimatorState.config.accumulator.interferenceAgeLimit);
        if (!state->pollutionAccumulator) {
            dbgf(estimatorState.dbgModule, DBGERR,
                 "%s: Failed to create pollution accumulator buffer", __func__);
            free(state);
            return NULL;
        }

        estimatorCmnCreatePollutionExpiryTimer(entry, state);
        stadbEntry_setEstimatorState(entry, state,
                                     estimatorCmnManageSTAStateLifecycleCB);
    }

    return state;
}

/**
 * @brief Lifecycle management function used to perform maintenance when the
 *        STA entry is being destroyed or rellocated.
 *
 * @param [in] handle  the entry being managed
 * @param [in] state  the estimator state object registered
 */
static void estimatorCmnManageSTAStateLifecycleCB(stadbEntry_handle_t handle,
                                                  void *state) {
    estimatorSTAState_t *statePtr = (estimatorSTAState_t *) state;

    unsigned remainingSecs, remainingUsecs;
    if (handle) {  // realloc, so record the remaining time
        evloopTimeoutRemaining(&statePtr->pollutionExpiryTimer, &remainingSecs,
                               &remainingUsecs);
    }

    // Cancel the timer in case it is running.
    evloopTimeoutUnregister(&statePtr->pollutionExpiryTimer);

    if (!handle) {  // destroy
        estimatorCircularBufferDestroy(statePtr->pollutionAccumulator);
        free(state);
    } else {  // realloc
        estimatorCmnCreatePollutionExpiryTimer(handle, statePtr);

        if (remainingSecs || remainingUsecs) {
            evloopTimeoutRegister(&statePtr->pollutionExpiryTimer,
                                  remainingSecs, remainingUsecs);
        }
    }
}

/**
 * @brief Create a new timer for managing pollution expiry.
 *
 * @param [in] handle  the entry to which to associate the timer
 * @param [in] state  the state object that will own the timer
 */
static void estimatorCmnCreatePollutionExpiryTimer(stadbEntry_handle_t handle,
        estimatorSTAState_t *state) {
    evloopTimeoutCreate(&state->pollutionExpiryTimer,
            "estimatorPollutionExpiryTimeout",
            estimatorCmnPollutionExpiryTimeoutHandler,
            handle);
}

/**
 * @brief Callback function used to check if a given BSS is no longer polluted
 *        and clear it accordingly.
 *
 * @param [in] entry  handle to the STA
 * @param [in] bssHandle  the stats for a given BSS
 * @param [in] cookie  the state information that should be updated
 *
 * @return LBD_FALSE always (as there is no need to keep the BSSes as a result
 *         of the iteration)
 */
static LBD_BOOL estimatorCmnPollutionCheckCallback(
        stadbEntry_handle_t entry, stadbEntry_bssStatsHandle_t bssHandle,
        void *cookie) {
    estimatorCmnPollutionCheckParams_t *params =
        (estimatorCmnPollutionCheckParams_t *) cookie;

    const struct ether_addr *staAddr = stadbEntry_getAddr(entry);
    lbDbgAssertExit(estimatorState.dbgModule, staAddr);

    const lbd_bssInfo_t *bssInfo =
        stadbEntry_resolveBSSInfo(bssHandle);
    lbDbgAssertExit(estimatorState.dbgModule, bssInfo);

    LBD_BOOL isPolluted;
    time_t expirySecs;
    if (stadbEntry_getPolluted(entry, bssHandle, &isPolluted,
                &expirySecs) != LBD_OK) {
        dbgf(estimatorState.dbgModule, DBGERR,
                "%s: Failed to get polluted state for " lbMACAddFmt(":") " on "
                lbBSSInfoAddFmt(), __func__,
                lbMACAddData(staAddr->ether_addr_octet),
                lbBSSInfoAddData(bssInfo));
        return LBD_FALSE;
    }

    if (isPolluted) {
        if (expirySecs) {
            params->numPolluted++;
            if (expirySecs < params->minExpiryDeltaSecs) {
                params->minExpiryDeltaSecs = expirySecs;
            }
        } else {  // Clear it as it is no longer polluted.
            if (stadbEntry_clearPolluted(entry, bssHandle) == LBD_OK) {
                dbgf(estimatorState.dbgModule, DBGINFO,
                        "%s: Cleared polluted state for " lbMACAddFmt(":")
                        " on " lbBSSInfoAddFmt(),
                        __func__, lbMACAddData(staAddr->ether_addr_octet),
                        lbBSSInfoAddData(bssInfo));
                params->numCleared++;

                estimatorCmnDiaglogSTAPollutionChanged(
                        staAddr, bssInfo, LBD_FALSE /* polluted */,
                        estimatorPollutionChangedReason_aging);
                estimatorCmnGeneratePollutionClearEvent(staAddr);
            } else {
                dbgf(estimatorState.dbgModule, DBGERR,
                        "%s: Failed to clear polluted state for " lbMACAddFmt(":")
                        " on " lbBSSInfoAddFmt(),
                        __func__, lbMACAddData(staAddr->ether_addr_octet),
                        lbBSSInfoAddData(bssInfo));
            }
        }
    }
    return LBD_FALSE;
}

/**
 * @brief React to the pollution timer expiring for a given STA.
 *
 * This will clear the pollution state of any BSS (local or remote) that
 * has reached its expiry time.
 *
 * @param [in] cookie  the handle to the stadb entry
 */
static void estimatorCmnPollutionExpiryTimeoutHandler(void *cookie) {
    stadbEntry_handle_t entry = (stadbEntry_handle_t) cookie;
    lbDbgAssertExit(estimatorState.dbgModule, entry);

    estimatorSTAState_t *state =
        (estimatorSTAState_t *) stadbEntry_getEstimatorState(entry);
    lbDbgAssertExit(estimatorState.dbgModule, state);

    estimatorCmnPollutionCheckParams_t params;
    params.numPolluted = 0;
    params.numCleared = 0;
    params.minExpiryDeltaSecs = INT32_MAX;

    if (stadbEntry_iterateBSSStats(entry,
                estimatorCmnPollutionCheckCallback,
                &params, NULL, NULL) != LBD_OK) {
        const struct ether_addr *addr = stadbEntry_getAddr(entry);
        dbgf(estimatorState.dbgModule, DBGERR,
                "%s: Failed to iterate over BSS stats for " lbMACAddFmt(":"),
                __func__, lbMACAddData(addr->ether_addr_octet));
    }

    if (params.numPolluted > 0) {
        evloopTimeoutRegister(&state->pollutionExpiryTimer,
                params.minExpiryDeltaSecs + 1, 0 /* usecs */);
    }
}

/**
 * @brief Read interference detection curve parameters from config file and create
 *        detection curve
 *
 * @return LBD_OK if all curves have been initialized successfully; otherwise return LBD_NOK
 */
static LBD_STATUS estimatorCmnInitInterferenceDetectionCurves(void) {
    estimatorCmnInterferenceDetectionCurveType_e curveType;
    size_t i = 0;
    size_t coefficientsSize =
        sizeof(estimatorCmnCurveElementDefaultTable[0]) /
        sizeof(estimatorCmnCurveElementDefaultTable[0][0]) - 1; // Exclude NULL line
    estimatorInterferenceDetectionCurveCoefficient_t coefficients[coefficientsSize];
    for (curveType = estimatorCmnInterferenceDetectionCurveType_24G_20M_1SS;
            curveType < estimatorCmnInterferenceDetectionCurveType_invalid;
            ++curveType) {
        struct profileElement *curveDefaultTable =
            estimatorCmnCurveElementDefaultTable[curveType];
        for (i = 0; i < sizeof(coefficients) / sizeof(coefficients[0]); ++i) {
            coefficients[i] =
                profileGetOptsFloat(mdModuleID_Estimator,
                        curveDefaultTable[i].Element,
                        curveDefaultTable);
        }

        if (LBD_NOK == estimatorInterferenceDetectionCurveInit(
                    &estimatorCmnInterferenceDetectionCurves[curveType],
                    coefficients[0], coefficients[1], coefficients[2],
                    coefficients[3], coefficients[4], coefficients[5])) {
            // Should never happen on target for now
            dbgf(estimatorState.dbgModule, DBGERR,
                    "%s: Failed to initialize interference detection curve for type %u "
                    "with coefficients: {%f, %f, %f, %f, %f, %f}",
                    __func__, curveType, coefficients[0], coefficients[1], coefficients[2],
                    coefficients[3], coefficients[4], coefficients[5]);
            return LBD_NOK;
        }
    }

    return LBD_OK;
}

/**
 * @brief Handle STA stats reported from driver
 *
 * @pre It has been confirmed that we are interested in detecting interference
 *      for the given client
 *
 * @param [in] entry  the handle to the given STA
 * @param [in] staAddr  MAC address of the given STA
 * @param [in] servingBSS  the handle to the serving BSS
 * @param [in] servingBSSInfo  the basic info of the serving BSS
 * @param [in] staStats  the statistics reported by firmware
 *
 * @return LBD_OK if proper decision has been maded based on the stats (
 *         detected/non-detected/ignore); otherwise return LBD_NOK
 */
static LBD_STATUS estimatorCmnHandleSTAStats(stadbEntry_handle_t entry,
        const struct ether_addr *staAddr,
        stadbEntry_bssStatsHandle_t servingBSS,
        const lbd_bssInfo_t *servingBSSInfo,
        const wlanif_staStats_t *staStats) {
    estimatorSTAState_t *state = estimatorCmnGetOrCreateSTAState(entry);
    if (!state) {
        dbgf(estimatorState.dbgModule, DBGERR,
                "%s: Failed to create state for " lbMACAddFmt(":"),
                __func__, lbMACAddData(staAddr->ether_addr_octet));
        return LBD_NOK;
    }

    // Check if goodput is not zero
    if (staStats->txByteCount != state->lastInterferenceSample.txByteCount) {
        u_int64_t byteCountDelta = 0;
        u_int32_t packetCountDelta = 0;
        if (staStats->txByteCount > state->lastInterferenceSample.txByteCount &&
                staStats->txPacketCount > state->lastInterferenceSample.txPacketCount) {
            byteCountDelta = staStats->txByteCount -
                state->lastInterferenceSample.txByteCount;
            packetCountDelta = staStats->txPacketCount -
                state->lastInterferenceSample.txPacketCount;

            estimatorCmnDiaglogIASStats(staAddr, servingBSSInfo, staStats->rssi,
                    staStats->txRate, byteCountDelta,
                    packetCountDelta);
        } // else wraparound, wait for next sample

        state->lastInterferenceSample = *staStats;

        if (byteCountDelta < estimatorState.config.iasMinDeltaBytes) {
            dbgf(estimatorState.dbgModule, DBGDUMP,
                    "%s: Ignore sample for " lbMACAddFmt(":") " since byte count increase "
                    "(%"PRIu64") does not meet threshold (%"PRIu64")",
                    __func__, lbMACAddData(staAddr->ether_addr_octet), byteCountDelta,
                    estimatorState.config.iasMinDeltaBytes);
            return LBD_OK;
        }
        if (packetCountDelta < estimatorState.config.iasMinDeltaPackets) {
            dbgf(estimatorState.dbgModule, DBGDUMP,
                    "%s: Ignore sample for " lbMACAddFmt(":") " since packet count increase "
                    "(%u) does not meet threshold (%u)",
                    __func__, lbMACAddData(staAddr->ether_addr_octet), packetCountDelta,
                    estimatorState.config.iasMinDeltaPackets);
            return LBD_OK;
        }
    } else {
        dbgf(estimatorState.dbgModule, DBGDUMP,
                "%s: Ignore sample for " lbMACAddFmt(":") " since goodput is zero",
                __func__, lbMACAddData(staAddr->ether_addr_octet));
        return LBD_OK;
    }

    if (staStats->rssi < estimatorState.config.iasLowRSSIThreshold) {
        dbgf(estimatorState.dbgModule, DBGDUMP,
                "%s: Ignore sample for " lbMACAddFmt(":") " since rssi (%d) is too low",
                __func__, lbMACAddData(staAddr->ether_addr_octet), staStats->rssi);
        return LBD_OK;
    }

    wlanif_phyCapInfo_t servingBSSPhy, minPhyCap;
    if (LBD_NOK == wlanif_getBSSPHYCapInfo(servingBSSInfo, &servingBSSPhy) ||
            !servingBSSPhy.valid) {
        dbgf(estimatorState.dbgModule, DBGERR,
                "%s: Failed to get PHY capabilities of BSS " lbBSSInfoAddFmt(),
                __func__, lbBSSInfoAddData(servingBSSInfo));
        return LBD_NOK;
    } else if (LBD_NOK == estimatorCmnResolveMinPhyCap(
                entry, staAddr, servingBSS, servingBSSInfo,
                &servingBSSPhy, &minPhyCap)) {
        return LBD_NOK;
    }

    wlanif_band_e servingBand = wlanif_resolveBandFromFreq(servingBSSInfo->freq);
    lbDbgAssertExit(estimatorState.dbgModule, servingBand != wlanif_band_invalid);

    lbd_linkCapacity_t maxRate;
    u_int8_t numSpatialStreams;
    if (minPhyCap.numStreams == 1 || stadbEntry_isMUMIMOSupported(entry)) {
        numSpatialStreams = 1;
    } else {
        // Cap the number of spatial streams to 2 (the 2SS curve will be used
        // for any number of streams 2 or greater)
        numSpatialStreams = 2;
    }
    estimatorCmnInterferenceDetectionCurveType_e curveType =
        estimatorCmnResolveInterferenceDetectionCurveTypeAndMaxRate(
                servingBand, numSpatialStreams, &minPhyCap, &maxRate);
    if (curveType == estimatorCmnInterferenceDetectionCurveType_invalid) {
        // Should never happen on target
        dbgf(estimatorState.dbgModule, DBGERR,
                "%s: Cannot find proper interference detection curve for " lbMACAddFmt(":")
                " on band %d, PHY capabilities (%u, %u, %u, %u, %u)",
                __func__, lbMACAddData(staAddr->ether_addr_octet), servingBand,
                minPhyCap.maxChWidth, minPhyCap.numStreams, minPhyCap.phyMode,
                minPhyCap.maxMCS, minPhyCap.maxTxPower);
        return LBD_NOK;
    }

    LBD_BOOL interferenceDetected = LBD_FALSE;
    if (LBD_NOK == estimatorInterferenceDetectionCurveEvaluate(
                &estimatorCmnInterferenceDetectionCurves[curveType],
                staStats->txRate, staStats->rssi, maxRate,
                &interferenceDetected)) {
        dbgf(estimatorState.dbgModule, DBGERR,
                "%s: Failed to evaluate interference detection for " lbMACAddFmt(":")
                " with curve type %u, RSSI %u, MCS %u",
                __func__, lbMACAddData(staAddr->ether_addr_octet), curveType, staStats->rssi,
                staStats->txRate);
        return LBD_NOK;
    }

    dbgf(estimatorState.dbgModule, interferenceDetected ? DBGDEBUG : DBGDUMP,
            "%s: Interference %s for STA " lbMACAddFmt(":") " on " lbBSSInfoAddFmt()
            " (RSSI %u, MCS %u, MaxRate %u)",
            __func__, interferenceDetected ? "detected" : "not detected",
            lbMACAddData(staAddr->ether_addr_octet), lbBSSInfoAddData(servingBSSInfo),
            staStats->rssi, staStats->txRate, maxRate);

    return estimatorCmnAddInterferenceSample(entry, staAddr, servingBSS, servingBSSInfo,
            state, interferenceDetected);
}

/**
 * @brief Record interference detection state for a given STA
 *
 * If it meets pollution detect/clear requirements, the pollution state will
 * be updated accordingly
 *
 * @param [in] entry  the handle to the given STA
 * @param [in] staAddr  MAC address of the given STA
 * @param [in] servingBSS  the handle to the serving BSS
 * @param [in] servingBSSInfo  the basic info of the serving BSS
 * @param [in] state  the estimator state object of the given STA
 * @param [in] detected  whether interference was detected or not
 */
static LBD_STATUS estimatorCmnAddInterferenceSample(stadbEntry_handle_t entry,
        const struct ether_addr *staAddr,
        stadbEntry_bssStatsHandle_t servingBSS,
        const lbd_bssInfo_t *servingBSSInfo,
        estimatorSTAState_t *state,
        LBD_BOOL detected) {
    estimatorCmnAccumulatorObserverCBParams_t params;
    params.entry = entry;
    params.staAddr = staAddr;
    params.servingBSS = servingBSS;
    params.servingBSSInfo = servingBSSInfo;

    estimatorCmnDiaglogSTAInterferenceDetected(staAddr, params.servingBSSInfo,
            detected);

    if (stadbEntry_getPolluted(entry, params.servingBSS,
                &params.prevPolluted, NULL) != LBD_OK) {
        dbgf(estimatorState.dbgModule, DBGERR,
                "%s: Failed to get polluted state for "
                lbMACAddFmt(":") " on BSS " lbBSSInfoAddFmt(),
                __func__, lbMACAddData(staAddr),
                lbBSSInfoAddData(params.servingBSSInfo));
        return LBD_NOK;
    }

    if (!lbAreBSSesSame(params.servingBSSInfo, &state->lastIntfDetectBSSInfo)) {
        // Serving BSS changed, should start a fresh pollution detection
        lbCopyBSSInfo(params.servingBSSInfo, &state->lastIntfDetectBSSInfo);
        if (LBD_NOK == estimatorPollutionAccumulatorReset(
                    state->pollutionAccumulator, params.prevPolluted,
                    &estimatorState.config.accumulator)) {
            // It should never fail on target
            dbgf(estimatorState.dbgModule, DBGERR,
                    "%s: Failed to reset pollution accumulator for " lbMACAddFmt(":"),
                    __func__, lbMACAddData(staAddr));
            return LBD_NOK;
        }
    }

    return estimatorPollutionAccumulatorAccumulate(
            state->pollutionAccumulator, detected, params.prevPolluted,
            &estimatorState.config.accumulator,
            estimatorCmnAccumulatorObserverCB, &params);
}

/**
 * @brief Callback function to handle pollution state change
 *
 * @param [in] pollutionState  the pollution change state
 * @param [in] cookie  @see estimatorCmnAccumulatorObserverCBParams_t
 *
 * @return LBD_NOK if failed to set/clear pollution state; otherwise
 *         return LBD_OK
 */
static LBD_STATUS estimatorCmnAccumulatorObserverCB(
        estimatorPollutionState_e pollutionState, void *cookie) {
    estimatorCmnAccumulatorObserverCBParams_t *params =
        (estimatorCmnAccumulatorObserverCBParams_t *)cookie;

    LBD_BOOL polluted;
    switch (pollutionState) {
        case estimatorPollutionState_detected:
            polluted = LBD_TRUE;
            break;
        case estimatorPollutionState_cleared:
            polluted = LBD_FALSE;
            break;
        default:
            // Do nothing if no valid pollution change state
            return LBD_OK;
    }

    return estimatorCmnUpdatePollutionState(params->staAddr, params->entry,
            params->servingBSS, params->servingBSSInfo,
            polluted, params->prevPolluted);
}

/**
 * @brief Update the interference detection state for a given STA.
 *
 * This will also emit a log at info level if the state is actually different
 * from the previously stored state. Otherwise, it will only emit at dump
 * level.
 *
 * @param [in] staAddr  the address of the STA
 * @param [in] entryHandle  the STA being updated
 * @param [in] servingBSS  the handle to the currently serving BSS
 * @param [in] servingBSSInfo  the currently serving BSS for the STA
 * @param [in] polluted  whether the serving BSS should be marked as polluted or not
 * @param [in] prevPolluted  the previous polluted state
 *
 * @return LBD_OK on success; LBD_NOK on failure
 */
static LBD_STATUS estimatorCmnUpdatePollutionState(
        const struct ether_addr *staAddr, stadbEntry_handle_t entry,
        stadbEntry_bssStatsHandle_t servingBSS,
        const lbd_bssInfo_t *servingBSSInfo, LBD_BOOL polluted,
        LBD_BOOL prevPolluted) {
    if (polluted) {
        if (stadbEntry_setPolluted(
                    entry, servingBSS,
                    estimatorState.config.maxPollutionTime) != LBD_OK) {
            dbgf(estimatorState.dbgModule, DBGERR,
                    "%s: Failed to set polluted state for "
                    lbMACAddFmt(":") " on BSS " lbBSSInfoAddFmt(),
                    __func__, lbMACAddData(staAddr), lbBSSInfoAddData(servingBSSInfo));
            return LBD_NOK;
        }

        // Schedule the pollution expiry timer if it was not already running
        // or its expiry time is beyond what this one would be.
        estimatorCmnStartPollutionTimer(
                staAddr, entry, estimatorState.config.maxPollutionTime + 1);
    } else {
        if (stadbEntry_clearPolluted(entry, servingBSS) != LBD_OK) {
            dbgf(estimatorState.dbgModule, DBGERR,
                    "%s: Failed to clear polluted state for "
                    lbMACAddFmt(":") " on BSS " lbBSSInfoAddFmt(),
                    __func__, lbMACAddData(staAddr), lbBSSInfoAddData(servingBSSInfo));
            return LBD_NOK;
        }
    }

    if (prevPolluted != polluted) {
        estimatorCmnDiaglogSTAPollutionChanged(staAddr, servingBSSInfo, polluted,
                estimatorPollutionChangedReason_detection);
        if (polluted) {
            // Generate an event the first time we detect interference so that
            // other modules can try to steer the STA.
            estimator_staInterferenceDetectedEvent_t event;
            lbCopyMACAddr(staAddr->ether_addr_octet, event.addr.ether_addr_octet);
            mdCreateEvent(mdModuleID_Estimator, mdEventPriority_Low,
                    estimator_event_staInterferenceDetected,
                    &event, sizeof(event));
        } else {
            estimatorCmnGeneratePollutionClearEvent(staAddr);
        }
    }

    dbgf(estimatorState.dbgModule,
            prevPolluted != polluted ? DBGINFO : DBGDUMP,
            "%s: Set pollution state to %d for " lbMACAddFmt(":") " on "
            lbBSSInfoAddFmt(), __func__, polluted,
            lbMACAddData(staAddr->ether_addr_octet),
            lbBSSInfoAddData(servingBSSInfo));

    return LBD_OK;
}

/**
 * @brief Resolve interference detection curve type from client's capability
 *
 * @param [in] band  which band the client is associated on
 * @param [in] numSpatialStreams  number of spatial streams to
 *                                use to determine the curve
 * @param [in] phyCap  the minimum PHY capabilities between client and serving BSS
 * @param [out] maxRate  the rate above which interference should assumed to
 *                       be not present
 *
 * @return the curve type to be used to evaluate interference
 */
static estimatorCmnInterferenceDetectionCurveType_e
estimatorCmnResolveInterferenceDetectionCurveTypeAndMaxRate(
        wlanif_band_e band, u_int8_t numSpatialStreams, const wlanif_phyCapInfo_t *phyCap,
        lbd_linkCapacity_t *maxRate) {
    estimatorCmnInterferenceDetectionCurveType_e type =
        estimatorCmnInterferenceDetectionCurveType_invalid;

    // Note: If the client supports MU-MIMO, use 1 spatial stream curve,
    // even if the client supports 2.  This is because we don't know
    // if the MU-MIMO scheduler will choose to use 1 or 2 spatial streams,
    // which may lead to low rates, which will be incorrectly labelled as
    // due to interference.

    wlanif_chwidth_e chwidth;
    if (band == wlanif_band_24g) {
        chwidth = wlanif_chwidth_20;
        if (numSpatialStreams == 1) {
            type = estimatorCmnInterferenceDetectionCurveType_24G_20M_1SS;
        } else {
            // For 2 and 2+ SS, use 2 SS curve
            type = estimatorCmnInterferenceDetectionCurveType_24G_20M_2SS;
        }
    } else { // 5 GHz
        switch (phyCap->maxChWidth) {
            case wlanif_chwidth_20:
                chwidth = wlanif_chwidth_20;
                if (numSpatialStreams == 1) {
                    type = estimatorCmnInterferenceDetectionCurveType_24G_20M_1SS;
                } else {
                    type = estimatorCmnInterferenceDetectionCurveType_24G_20M_2SS;
                }
                break;
            case wlanif_chwidth_40:
                chwidth = wlanif_chwidth_40;
                if (numSpatialStreams == 1) {
                    type = estimatorCmnInterferenceDetectionCurveType_5G_40M_1SS;
                } else {
                    type = estimatorCmnInterferenceDetectionCurveType_5G_40M_2SS;
                }
                break;
            default:
                chwidth = wlanif_chwidth_80;
                if (numSpatialStreams == 1) {
                    type = estimatorCmnInterferenceDetectionCurveType_5G_80M_1SS;
                } else {
                    type = estimatorCmnInterferenceDetectionCurveType_5G_80M_2SS;
                }
        }
    }

    u_int32_t phyRateMax =
        estimatorSNRToPhyRateTablePerformLookup(
                estimatorState.dbgModule, phyCap->phyMode, chwidth, numSpatialStreams,
                phyCap->maxMCS, LBD_MAX_SNR);
    if ( phyRateMax == LBD_INVALID_LINK_CAP ) {
        dbgf(estimatorState.dbgModule, DBGERR,"%s: Failed to get valid PhyRate using PhyMode [%u] "
        "chWidth [%u] NumStreams [%u] MaxMCS [%u] ", __func__, phyCap->phyMode, chwidth,
        numSpatialStreams, phyCap->maxMCS);
        return estimatorCmnInterferenceDetectionCurveType_invalid;
    }
    *maxRate = ((phyRateMax * estimatorState.config.iasMaxRateFactor) / 100);
    return type;
}

#ifndef GMOCK_UNIT_TESTS
static
#endif
wlanif_chwidth_e estimatorCmnResolveChannelBandwidth(stadbEntry_handle_t entry) {
    stadbEntry_bssStatsHandle_t servingBSS = stadbEntry_getServingBSS(entry, NULL);
    if (!servingBSS) {
        return wlanif_chwidth_20;
    }

    wlanif_phyCapInfo_t phyCapInfo;
    if (LBD_OK != stadbEntry_getPHYCapInfo(entry, servingBSS, &phyCapInfo)) {
        return wlanif_chwidth_20;
    }

    return phyCapInfo.maxChWidth;
}

int8_t estimator_convertRSSIFromDbToDbm(stadbEntry_handle_t entry, lbd_rssi_t rssi) {
    wlanif_chwidth_e chanWidth = estimatorCmnResolveChannelBandwidth(entry);
    return estimatorRCPIToPhyRateConvertRSSIFromDbToDbm(estimatorState.dbgModule,
            rssi, chanWidth);
}

lbd_rssi_t estimator_convertRSSIFromDbmToDb(stadbEntry_handle_t entry, int8_t rssi) {
    wlanif_chwidth_e chanWidth = estimatorCmnResolveChannelBandwidth(entry);
    return estimatorRCPIToPhyRateConvertRSSIFromDbmToDb(estimatorState.dbgModule,
            rssi, chanWidth);
}

/**
 * @brief Start/update the timer for 802.11k events.
 *
 * If the timer is already running and the expiry for this state is less than
 * the currently scheduled expiry, the timer is rescheduled.
 *
 * @param [in] state  the internal state object for the STA for which to start
 *                    the timer
 * @param [in] durationSecs  the amount of time for the timer (in seconds)
 * @param [in] updateStateOnly  only update internal state object for the STA,
 *                              not try to schedule timer
 */
static void estimatorCmnStart11kTimeout(estimatorSTAState_t *state,
                                        unsigned durationSecs,
                                        LBD_BOOL updateStateOnly) {
    lbGetTimestamp(&state->dot11kTimeout);
    state->dot11kTimeout.tv_sec += durationSecs;

    if (updateStateOnly) { return; }

    estimatorState.numDot11kTimers++;

    if (estimatorState.numDot11kTimers == 1 ||
        lbIsTimeBefore(&state->dot11kTimeout,
                       &estimatorState.nextDot11kExpiry)) {
        // The + 1 is to ensure that we do not get an early expiry that causes
        // us to just quickly reschedule a 0 second timer.
        evloopTimeoutRegister(&estimatorState.dot11kTimer,
                              durationSecs + 1, 0);
        estimatorState.nextDot11kExpiry = state->dot11kTimeout;
    }  // else timer is already running and is shorter than this state needs
}

static void estimatorCmnStart11kReqTimeout(estimatorSTAState_t *state,
                                           unsigned durationSecs,
                                           LBD_BOOL updateStateOnly) {
    lbGetTimestamp(&state->dot11kReqTimeout);
    state->dot11kReqTimeout.tv_sec += durationSecs;

    if (updateStateOnly) { return; }

    estimatorState.numDot11kReqTimers++;

    if (estimatorState.numDot11kReqTimers >= 1 ||
        lbIsTimeBefore(&state->dot11kReqTimeout,
                       &estimatorState.nextDot11kReqExpiry)) {
        // The + 1 is to ensure that we do not get an early expiry that causes
        // us to just quickly reschedule a 0 second timer.
        evloopTimeoutRegister(&estimatorState.dot11kReqTimer,
                              durationSecs + 1, 0);
        estimatorState.nextDot11kReqExpiry = state->dot11kReqTimeout;
    }  // else timer is already running and is shorter than this state needs
}


/**
 * @brief Start the timer that waits for an 802.11k beacon report response.
 *
 * @param [in] state  the internal state object for the STA for which to start
 *                    the timer
 */
static void estimatorCmnStart11kResponseTimeout(estimatorSTAState_t *state) {
    state->dot11kState = estimator11kState_awaiting11kBeaconReport;
    estimatorCmnStart11kTimeout(state, estimatorState.config.max11kResponseTime,
                                LBD_FALSE /* updateStateOnly */);
}

static void estimatorCmnStart11kRequestTimeout(estimatorSTAState_t *state) {
    estimatorCmnStart11kReqTimeout(state, estimatorState.config.max11kRequestTime,
                                LBD_FALSE /* updateStateOnly */);
}

/**
 * @brief Start the timer that waits for enough time to elapse to allow for
 *        another 802.11k request.
 *
 * If the last 802.11k measurement was successful and the client was idle
 * before 802.11k request was sent, use longer timeout; otherwise,
 * use shorter timeout to increase STA's opportunity to be steered.
 *
 * @param [in] state  the internal state object for the STA for which to start
 *                    the timer
 * @param [in] dot11kResult  whether the 11k measurement was successful or not
 * @param [in] updateStateOnly  only update internal state object for the STA, but
 *                              not try to manipulate timer
 * @param [in] clientClassGroup  the client classification group the sta is operating at
 */
static void estimatorCmnStart11kProhibitTimeout(estimatorSTAState_t *state,
                                                LBD_STATUS dot11kResult,
                                                LBD_BOOL updateStateOnly,
                                                u_int8_t clientClassGroup) {
    state->dot11kState = estimator11kState_awaiting11kProhibitExpiry;
    unsigned prohibitDurationSecs = estimatorState.config.dot11kProhibitTimeShort[clientClassGroup];
    if (dot11kResult == LBD_OK && !state->activeBefore11k) {
        prohibitDurationSecs = estimatorState.config.dot11kProhibitTimeLong[clientClassGroup];
    }

    estimatorCmnStart11kTimeout(state, prohibitDurationSecs, updateStateOnly);
}

/**
 * @brief Mark the 802.11k measurement as complete.
 *
 * This will generate the event so other modules know it is complete and start
 * the necessary timer to throttle repeated 802.11k measurements.
 *
 * @param [in] state  the internal state object for the STA for which to start
 *                    the timer
 * @param [in] addr  the address which was completed
 * @param [in] result  whether the measurement was successful or not
 * @param [in] channel  the channel measured in 802.11k beacon report
 * @param [in] localBSS  the local BSS reported in 802.11k beacon report
 * @param [in] startProhibitTimer  whether to start 11k prohibit timer
 */
static void estimatorCmnCompleteDot11kMeasurement(estimatorSTAState_t *state,
                                                  const struct ether_addr *addr,
                                                  LBD_STATUS result,
                                                  lbd_channelId_t channel,
                                                  u_int16_t freq,
                                                  const lbd_bssInfo_t *localBSS,
                                                  LBD_BOOL startTimer) {
    // Let steeralg know that the data is ready or that it failed.
    estimator_staDataMetricsCompleteEvent_t event;
    lbCopyMACAddr(addr->ether_addr_octet, event.addr.ether_addr_octet);
    event.result = result;
    event.trigger = state->trigger;
    event.measuredChannel = channel;
    event.measuredFreq = freq;
    if (result == LBD_OK && localBSS) {
        lbCopyBSSInfo(localBSS, &event.measuredServingAPBSS);
    } else {
        memset(&event.measuredServingAPBSS, 0, sizeof(event.measuredServingAPBSS));
        event.measuredServingAPBSS.channelId = LBD_CHANNEL_INVALID;
        event.measuredServingAPBSS.freq = LBD_FREQ_INVALID;
    }

    mdCreateEvent(mdModuleID_Estimator, mdEventPriority_Low,
                  estimator_event_staDataMetricsComplete,
                  &event, sizeof(event));

    if (result == LBD_OK) {
        // Reset failure count
        state->countConsecutive11kFailure = 0;
    } else {
        // Increment failure count
        state->countConsecutive11kFailure++;
    }

    if (state->countConsecutive11kFailure >= estimatorState.config.max11kUnfriendly) {
        dbgf(estimatorState.dbgModule, DBGDEBUG,
             "%s: Maximum (%u) consecutive 11k measurement failures reported (%u) for STA "
             lbMACAddFmt(":") " , attempting legacy steering", __func__,
             estimatorState.config.max11kUnfriendly, state->countConsecutive11kFailure,
             lbMACAddData(event.addr.ether_addr_octet));

        mdCreateEvent(mdModuleID_Estimator, mdEventPriority_Low,
                    estimator_event_requestLegacySteering11kSta,
                    &event, sizeof(event));
    }

    u_int8_t clientClassGroup = 0;
    stadbEntry_handle_t entry = stadb_find(addr);
    stadbEntry_getClientClassGroup(entry, &clientClassGroup);
    stadbEntry_resetBeaconReport(entry);

    // Throttle the next 11k measurement to prevent clients from
    // getting unhappy with the AP requesting too many measurements.
    estimatorCmnStart11kProhibitTimeout(state, result, !startTimer, clientClassGroup);
}

/**
 * @brief If this is a BSS on the serving AP on the same band as the serving
 *        BSS, store its rate and airtime (estimated).
 *
 * @param [in] entryHandle  the STA being processed
 * @param [in] bssHandle  the BSS for which to update the RSSI (if necesasry)
 * @param [in] cookie  the internal parameters for the iteration
 *
 * @return LBD_FALSE always (as it does not keep the BSSes around)
 */
static LBD_BOOL estimatorServingBandRateAirtimeCallback(
        stadbEntry_handle_t entryHandle, stadbEntry_bssStatsHandle_t bssHandle,
        void *cookie) {
    estimatorServingBandRateAirtimeParams_t *params =
        (estimatorServingBandRateAirtimeParams_t *) cookie;
    lbDbgAssertExit(estimatorState.dbgModule, params);

    const lbd_bssInfo_t *targetBSSInfo = stadbEntry_resolveBSSInfo(bssHandle);
    lbDbgAssertExit(estimatorState.dbgModule, targetBSSInfo);

    stadbEntry_bssStatsHandle_t servingBSS;
    const lbd_bssInfo_t *servingBSSInfo;
    if (!estimatorCmnIsSameServingAP(entryHandle, targetBSSInfo->apId, &servingBSS,
                                     &servingBSSInfo) ||
        (servingBSS == bssHandle)) {
        // Ignore BSSes on other APs and the currently serving BSS.
        return LBD_FALSE;
    }

    const wlanif_band_e bssBand =
        wlanif_resolveBandFromFreq(targetBSSInfo->freq);

    if (params->servingTxRate == LBD_INVALID_LINK_CAP ||
        bssBand != params->servingBand) {
        // No recent serving band capacity information or it is another band
        // than the serving one.
        return LBD_FALSE;
    }

    lbd_linkCapacity_t phyRate = params->servingTxRate;
    if (params->servingSNR != LBD_INVALID_SNR) {
        wlanif_phyCapInfo_t phyInfo = { LBD_FALSE /* valid */};
        if (LBD_NOK == wlanif_getBSSPHYCapInfo(targetBSSInfo, &phyInfo) ||
            !phyInfo.valid) {
            dbgf(estimatorState.dbgModule, DBGERR,
                 "%s: Unable to resolve PHY capabilities for " lbMACAddFmt(":")
                 " on " lbBSSInfoAddFmt(),
                 __func__, lbMACAddData(params->staAddr->ether_addr_octet),
                 lbBSSInfoAddData(targetBSSInfo));
        } else {
            lbd_snr_t adjustedSNR =
                params->servingSNR + phyInfo.maxTxPower - params->servingTxPower;
            wlanif_phyCapInfo_t minPHY;
            wlanif_resolveMinPhyCap(&phyInfo, &params->staPHY, &minPHY);
            // Adjust SNR based on max Tx power and look up estimated full link capacity
            phyRate = estimatorSNRToPhyRateTablePerformLookup(
                          estimatorState.dbgModule, minPHY.phyMode, minPHY.maxChWidth,
                          minPHY.numStreams, minPHY.maxMCS, adjustedSNR);
            dbgf(estimatorState.dbgModule, DBGDEBUG,
                 "%s: Estimated PHY rate %u Mbps with adjusted SNR %u dB for " lbMACAddFmt(":")
                 " on " lbBSSInfoAddFmt(), __func__, phyRate, adjustedSNR,
                 lbMACAddData(params->staAddr->ether_addr_octet), lbBSSInfoAddData(targetBSSInfo));
        }
    }

    // Compute the airtime and store it in the entry.
    LBD_STATUS result = estimatorCmnComputeAndStoreStats(
        entryHandle, params->staAddr,
        estimatorNonServingRateAirtimeFlags_airtime |
            estimatorNonServingRateAirtimeFlags_nonMeasured,
        bssHandle, phyRate, LBD_INVALID_RCPI);
    if (result != LBD_OK) {
        dbgf(estimatorState.dbgModule, DBGERR,
             "%s: Failed to write back capacity and airtime for "
             lbMACAddFmt(":") " on " lbBSSInfoAddFmt(),
             __func__,
             lbMACAddData(params->staAddr->ether_addr_octet),
             lbBSSInfoAddData(targetBSSInfo));
    }

    params->result |= result;  // will only be updated on failure
    return LBD_FALSE;
}

/**
 * @brief Compute a rate and airtime for all BSSes on the same band on the
 *        same AP as the serving one.
 *
 * @param [in] entry  the station database entry
 * @param [in] state  the estimator state for this entry
 * @param [in] staAddr  the MAC address of the STA being estimated
 *
 * @return LBD_OK on success; otherwise LBD_NOK
 */
static LBD_STATUS estimatorComputeServingBandRateAirtime(
        stadbEntry_handle_t entry, estimatorSTAState_t *state,
        const struct ether_addr *staAddr) {
    estimatorServingBandRateAirtimeParams_t servingParams;
    servingParams.result = LBD_OK;
    servingParams.staAddr = staAddr;

    servingParams.servingBand = wlanif_resolveBandFromFreq(
            state->statsEnabledBSSInfo.freq);

    stadbEntry_bssStatsHandle_t servingBSS = stadbEntry_getServingBSS(entry, NULL);
    if (LBD_NOK == stadbEntry_getPHYCapInfo(entry, servingBSS, &servingParams.staPHY)) {
        servingParams.staPHY.valid = LBD_FALSE;
    }

    // If this comes back as invalid or it is too old, we will not update
    // the other BSSes on the serving band. Generally this should not
    // occur and since steeralg checks the age, this should be acceptable.
    time_t age;
    stadbEntry_getFullCapacities(entry, servingBSS, NULL /*ulCapacity*/,
            &servingParams.servingTxRate, NULL /*deltaUlSecs*/, &age);
    if (servingParams.servingTxRate == LBD_INVALID_LINK_CAP ||
        age > estimatorState.config.ageLimit) {
        servingParams.servingTxRate = LBD_INVALID_LINK_CAP;
        servingParams.servingSNR = LBD_INVALID_SNR;
    } else {
        // If serving Tx rate cannot be mapped back to an SNR, will
        // use same Tx rate to update other BSSes on the serving band.
        wlanif_phyCapInfo_t servingBSSPhy = { LBD_FALSE /* valid */};
        if (LBD_NOK == wlanif_getBSSPHYCapInfo(&state->statsEnabledBSSInfo, &servingBSSPhy) ||
            !servingBSSPhy.valid) {
            dbgf(estimatorState.dbgModule, DBGERR,
                 "%s: Failed to get BSS PHY capability on " lbBSSInfoAddFmt(),
                 __func__, lbBSSInfoAddData(&state->statsEnabledBSSInfo));
            servingParams.servingSNR = LBD_INVALID_SNR;
        } else if (!servingParams.staPHY.valid) {
            dbgf(estimatorState.dbgModule, DBGERR,
                 "%s: Failed to get STA PHY capability for " lbMACAddFmt(":"),
                 __func__, lbMACAddData(staAddr->ether_addr_octet));
            servingParams.servingSNR = LBD_INVALID_SNR;
        } else {
            servingParams.servingTxPower = servingBSSPhy.maxTxPower;
            wlanif_phyCapInfo_t minPHY;
            wlanif_resolveMinPhyCap(&servingBSSPhy, &servingParams.staPHY, &minPHY);
            servingParams.servingSNR =
                estimatorSNRToPhyRateTablePerformReverseLookup(
                    estimatorState.dbgModule, minPHY.phyMode,
                    minPHY.maxChWidth, minPHY.numStreams,
                    minPHY.maxMCS, servingParams.servingTxRate);
            dbgf(estimatorState.dbgModule, DBGDEBUG,
                 "%s: Estimated SNR %u dB from PHY rate %u Mbps for " lbMACAddFmt(":")
                 " on " lbBSSInfoAddFmt(),
                 __func__, servingParams.servingSNR, servingParams.servingTxRate,
                 lbMACAddData(servingParams.staAddr->ether_addr_octet),
                 lbBSSInfoAddData(&state->statsEnabledBSSInfo));
        }

        if (servingParams.servingSNR == LBD_INVALID_SNR) {
            dbgf(estimatorState.dbgModule, DBGDEBUG,
                 "%s: No valid estimated SNR, will use measured Tx rate %u Mbps "
                 "for all serving band BSSes",
                 __func__, servingParams.servingTxRate);
        }
    }


    if (stadbEntry_iterateBSSStats(entry,
                                   estimatorServingBandRateAirtimeCallback,
                                   &servingParams, NULL, NULL) != LBD_OK) {
        dbgf(estimatorState.dbgModule, DBGERR,
             "%s: Failed to iterate over non-serving BSS stats",
             __func__);
        servingParams.result = LBD_NOK;
    }

    return servingParams.result;
}

/**
 * @brief Estimate the rate and airtime for the BSS provided.
 *
 * @param [in] entryHandle  the STA being processed
 * @param [in] bssHandle  the BSS for which to update the RSSI (if necesasry)
 * @param [in] cookie  the internal parameters for the iteration
 *
 * @return LBD_FALSE always (as it does not keep the BSSes around)
 */
static LBD_BOOL estimatorCmnNonServingRateAirtimeCallback(
    stadbEntry_handle_t entryHandle, stadbEntry_bssStatsHandle_t bssHandle,
    void *cookie) {
    estimatorNonServingRateAirtimeParams_t *params =
        (estimatorNonServingRateAirtimeParams_t *) cookie;
    lbDbgAssertExit(estimatorState.dbgModule, params);

    const lbd_bssInfo_t *targetBSSInfo = stadbEntry_resolveBSSInfo(bssHandle);
    lbDbgAssertExit(estimatorState.dbgModule, targetBSSInfo);

    if (wlanif_resolveBandFromFreq(targetBSSInfo->freq) !=
            params->measuredBand ||
        !lbAreBSSInfoSameAP(targetBSSInfo, params->measuredBss)) {
        // Ignored due to not the same band or not from same AP.
        return LBD_FALSE;
    }

    if (!(params->flags & estimatorNonServingRateAirtimeFlags_nonMeasured) &&
        bssHandle != params->measuredBSSStats) {
        // Ignored due to only flags that request only to estimate the
        // measured BSSes.
        return LBD_FALSE;
    }

    // We need to get both the AP and STA capabilities on the target
    // BSS so we can take the lowest common denominator.
    wlanif_phyCapInfo_t bssCap = { LBD_FALSE /* valid */};
    if (LBD_NOK == wlanif_getBSSPHYCapInfo(targetBSSInfo, &bssCap) ||
        !bssCap.valid) {
        dbgf(estimatorState.dbgModule, DBGERR,
             "%s: Failed to resolve BSS capabilities for " lbBSSInfoAddFmt(),
             __func__, lbBSSInfoAddData(targetBSSInfo));
        params->result = LBD_NOK;
        return LBD_FALSE;
    }

    if (LBD_NOK == estimatorCmnEstimateNonServingRateAirtime(
                       entryHandle, params->staAddr, params->flags,
                       params->measuredBSSStats, bssHandle, targetBSSInfo, &bssCap,
                       params->rcpi, params->txPower)) {
        params->result = LBD_NOK;
        return LBD_FALSE;
    } else {
        return LBD_TRUE;
    }
}

/**
 * @brief Process the beacon report and write the RCPI and PHY rate
 *        information back to the stadb.
 *
 * @param [in] entry  the STA that performed the measurement
 * @param [in] bcnrptEvent  the info provided by the STA
 * @param [in] flags  flags that determine whether the airtime and non-measured
 *                    BSSes are estimated or not
 * @param [out] reportedLocalBSS  the BSS reported for the STA's serving AP,
 *                                if any
 * @param [out] measuredChannel  the channel on which the measurement was
 *                               performed
 *
 * @return LBD_OK on success; otherwise LBD_NOK
 */
static LBD_STATUS estimatorCmnStoreBeaconReportEvent(
    stadbEntry_handle_t entry, const wlanif_beaconReport_t *bcnrptEvent,
    u_int8_t flags, const lbd_bssInfo_t **reportedLocalBss,
    lbd_channelId_t *measuredChannel, u_int16_t *measuredFreq) {
    if (!bcnrptEvent->valid) {
        dbgf(estimatorState.dbgModule, DBGERR,
             "%s: Invalid beacon report for " lbMACAddFmt(":"),
             __func__,
             lbMACAddData(bcnrptEvent->sta_addr.ether_addr_octet));
        return LBD_NOK;
    }

    const lbd_bssInfo_t *reportedBss = &bcnrptEvent->reportedBcnrptInfo[0].reportedBss;
    // Make sure the channel can be resolved to a band.
    wlanif_band_e measuredBand =
        wlanif_resolveBandFromFreq(reportedBss->freq);
    if (measuredBand == wlanif_band_invalid) {
        dbgf(estimatorState.dbgModule, DBGERR,
             "%s: Cannot resolve channel %u to band for " lbMACAddFmt(":"),
             __func__, reportedBss->channelId,
             lbMACAddData(bcnrptEvent->sta_addr.ether_addr_octet));
        return LBD_NOK;
    }
    *measuredChannel = reportedBss->channelId;
    *measuredFreq = reportedBss->freq;

    return estimatorHandleValidBeaconReport(entry, bcnrptEvent, flags,
                                            measuredBand, reportedLocalBss);
}


/**
 * @ClientComplianceRcpiCheck
 *
 * @param [in] Beaconreport event pointer
 * @param [in] Stadb Entry handle
 */
static void estimatorCmn11kClientComplianceRcpiCheck(
                           struct wlanif_beaconReportEvent_t *bcnrptEvent,
                           stadbEntry_handle_t entry) {
    size_t i = 0;

    if (bcnrptEvent->valid) {
        while (i < bcnrptEvent->numBcnrpt) {
        u_int8_t rcpi;
        u_int8_t clientRcpiType = LBD_RCPI_TYPE_UNKNOWN;

        stadbEntry_getClientRcpiType(entry, &clientRcpiType);

        rcpi = bcnrptEvent->reportedBcnrptInfo[i].rcpi;
        dbgf(estimatorState.dbgModule, DBGINFO,
              "%s: RCPI info for STA "lbMACAddFmt(":") " rcpi: %d, clientRcpiType:%d", __func__,
              lbMACAddData(bcnrptEvent->sta_addr.ether_addr_octet),
              bcnrptEvent->reportedBcnrptInfo[i].rcpi, clientRcpiType);
       /*
        Clients can send RCPI either as 2's compliment (non-spec) or as per spec value
        Use RCPI hex value 0xA6 (decimal value 166) as boundary to determine
        if RCPI should be
        interpreted as 2's compliment (non-spec) or converted as per spec equation.

        when 11k response is received on AP, it will do following
       1. Check stadb, if RcpiType is non-zero (i.e. 1)
          Interpret RCPI using 11k method, Rcpi type will be still 1
       2. If RcpiType is zero
          Check RCPI HEX value:
         a. If RCPI  0x00(0) ~ 0x91(145) – leave RcpiType as 1,
            Interpret RCPI value as 11k complaint method
         b. If RCPI  0x92(146) ~ 0xA5(165) – do not mark client, leave RcpiType as 0,
            Interpret RCPI value as 11K compliant method
         c. If RCPI 0xA6(166) ~  0xFF(255) – do not mark client, leave RcpiType as 0,
            Interpret RCPI value as 2’s compliment method
       */
        if (estimatorState.config.enableRcpiTypeClassification) { /* Enable Rcpi Type */
            if (clientRcpiType == LBD_RCPI_TYPE_11KCOMPLIANT) { /*rcpiType 1 */
                bcnrptEvent->reportedBcnrptInfo[i].rcpi = (rcpi/2) - 110;
                dbgf(estimatorState.dbgModule, DBGINFO,
                     "%s: Changing rcpi to negative as per spec:%d, clientRcpiType:%d", __func__,
                     bcnrptEvent->reportedBcnrptInfo[i].rcpi, clientRcpiType);

            } else { /*rcpiType 0 */
                   if (rcpi < estimatorState.config.rcpi11kNonCompliantDetectionThreshold) {
                       bcnrptEvent->reportedBcnrptInfo[i].rcpi = (rcpi/2) - 110;
                       if (rcpi < estimatorState.config.rcpi11kCompliantDetectionThreshold) {
                           stadbEntry_setClientRcpiType(entry, LBD_RCPI_TYPE_11KCOMPLIANT);
                           estimator_rcpiTypeChangedEvent_t event;

                           lbCopyMACAddr(bcnrptEvent->sta_addr.ether_addr_octet, event.addr.ether_addr_octet);
                           event.rcpiTypeChanged = LBD_RCPI_TYPE_11KCOMPLIANT;
                           mdCreateEvent(mdModuleID_Estimator, mdEventPriority_High,
                                         estimator_event_rcpiTypeChanged, &event, sizeof(event));
                       }
                       stadbEntry_getClientRcpiType(entry, &clientRcpiType);
                       dbgf(estimatorState.dbgModule, DBGINFO,
                            "%s: Changing rcpi to negative as per spec: %d, clientRcpiType:%d",
                             __func__, bcnrptEvent->reportedBcnrptInfo[i].rcpi, clientRcpiType);
                       } else {
                              bcnrptEvent->reportedBcnrptInfo[i].rcpi = rcpi;
                       }
                   }
        } else { /* Map Mode  */
               if (rcpi > 0xA0) {
                   bcnrptEvent->reportedBcnrptInfo[i].rcpi = rcpi;
               } else {
                       bcnrptEvent->reportedBcnrptInfo[i].rcpi = (rcpi/2) - 110;
                       dbgf(estimatorState.dbgModule, DBGINFO,
                            "%s: Changing rcpi to negative as per spec: %d, clientRcpiType:%d", __func__,
                             bcnrptEvent->reportedBcnrptInfo[i].rcpi, clientRcpiType);
               }
        }
        dbgf(estimatorState.dbgModule, DBGINFO, "%s: Interpreted RCPI Info for STA: "lbMACAddFmt(":")
             " " lbBSSInfoAddFmt() " rcpi: %d  clientRcpiType:%d ", __func__, lbMACAddData(bcnrptEvent->sta_addr.ether_addr_octet),
             lbBSSInfoAddData(&(bcnrptEvent->reportedBcnrptInfo[i].reportedBss)),
             (int8_t)bcnrptEvent->reportedBcnrptInfo[i].rcpi, clientRcpiType);

        i++;
        }
    }
}

/**
 * @brief React to 802.11k beacon report
 *
 * @param [in] event  the event carrying the beacon report
 */
static void estimatorCmnHandleBeaconReportEvent(struct mdEventNode *event) {
    wlanif_beaconReportEvent_t *bcnrptEvent =
        ( wlanif_beaconReportEvent_t *) event->Data;
    lbDbgAssertExit(estimatorState.dbgModule, bcnrptEvent);

    stadbEntry_handle_t entry = stadb_find(&bcnrptEvent->sta_addr);
    if (!entry) {
        dbgf(estimatorState.dbgModule, DBGERR,
             "%s: Beacon report for unknown STA " lbMACAddFmt(":"),
             __func__, lbMACAddData(bcnrptEvent->sta_addr.ether_addr_octet));
        return;
    }

    estimatorCmn11kClientComplianceRcpiCheck(bcnrptEvent,entry);
    wlanif_beaconReport_t bcnrpt = {0};
    u_int8_t pendingBcnRpt = stadbEntryUpdateBeaconReport(entry, bcnrptEvent);
    if (pendingBcnRpt) {
        dbgf(estimatorState.dbgModule, DBGDEBUG,
             "%s %d: Beacon reports for STA " lbMACAddFmt(":") " Pending %d numBcnRpt %zu",
             __func__, __LINE__, lbMACAddData(bcnrptEvent->sta_addr.ether_addr_octet), pendingBcnRpt, bcnrptEvent->numBcnrpt);
        return;
    } else {
        stadbEntryRetrieveBeaconReport(entry, &bcnrpt);
        dbgf(estimatorState.dbgModule, DBGDEBUG,
             "%s %d: Beacon reports for STA " lbMACAddFmt(":") " Pending %d numBcnRpt %zu",
             __func__, __LINE__, lbMACAddData(bcnrpt.sta_addr.ether_addr_octet), pendingBcnRpt, bcnrpt.numBcnrpt);
    }

    const lbd_bssInfo_t *reportedLocalBss = NULL;
    lbd_channelId_t measuredChannel = LBD_CHANNEL_INVALID;
    u_int16_t measuredFreq = LBD_FREQ_INVALID;

    estimatorSTAState_t *state = stadbEntry_getEstimatorState(entry);
    if (state) {
        if (estimator11kState_awaiting11kBeaconReport == state->dot11kState) {
            // No longer need the response timer.
            estimatorState.numDot11kTimers--;

            LBD_STATUS result = LBD_NOK;
            do {
                if (estimatorComputeServingBandRateAirtime(
                            entry, state, &bcnrpt.sta_addr) != LBD_OK) {
                    result = LBD_NOK;
                    break;
                }

                result = estimatorCmnStoreBeaconReportEvent(
                    entry, &bcnrpt,
                    estimatorNonServingRateAirtimeFlags_airtime |
                        estimatorNonServingRateAirtimeFlags_nonMeasured,
                    &reportedLocalBss, &measuredChannel, &measuredFreq);
            } while(0);

            estimatorCmnCompleteDot11kMeasurement(state, &bcnrpt.sta_addr,
                                                  result, measuredChannel,
                                                  measuredFreq,
                                                  reportedLocalBss,
                                                  LBD_TRUE /* startProhibitTimer */);
        } else if (estimator11kState_awaitingInd11kBeaconReport == state->dot11kState) {
            estimatorCmnStoreBeaconReportEvent(entry, &bcnrpt, 0 /* flags */,
                                               &reportedLocalBss, &measuredChannel, &measuredFreq );
            state->dot11kState = estimator11kState_idle;
        } else {
            dbgf(estimatorState.dbgModule, DBGERR,
                 "%s: Beacon report for STA " lbMACAddFmt(":")
                 " in unexpected state %u",
                 __func__, lbMACAddData(bcnrpt.sta_addr.ether_addr_octet),
                 state->dot11kState);
        }
    } else {
        // No state but there was a report. This should never happen.
        dbgf(estimatorState.dbgModule, DBGERR,
             "%s: Beacon report for STA " lbMACAddFmt(":")
             " with no state object",
             __func__, lbMACAddData(bcnrpt.sta_addr.ether_addr_octet));
    }
}

/**
 * @brief Handle updated STA stats pushed from the driver
 *
 * Evaluates whether these stats are relevant, and runs the
 * interference detector
 *
 * @param [in] addr  MAC address of STA for which stats are
 *                   received
 * @param [in] bss  BSS on which the stats are received
 * @param [in] stats  updated stats
 * @param [in] cookie  value provided when callback was
 *                     registered
 */
static void estimatorCmnHandleSTAStatsUpdate(
    const struct ether_addr *addr, const lbd_bssInfo_t *bss,
    const wlanif_staStats_t *stats, void *cookie) {
    if (estimatorState.debugModeEnabled) {
        // Ignore all stats update from driver when debug mode is enabled
        return;
    }

    if (!addr || !bss || !stats) {
        return;
    }

    // Make sure this is a STA for which we are interested in stats.
    // Must be:
    // - locally associated on bss
    // - dual band
    stadbEntry_handle_t entry = stadb_find(addr);
    if (!entry) {
        dbgf(estimatorState.dbgModule, DBGINFO,
             "%s: Ignoring stats for unknown STA " lbMACAddFmt(":"),
             __func__, lbMACAddData(addr->ether_addr_octet));
        return;
    }

    if (!estimatorState.config.iasEnableSingleBandDetect &&
        !stadbEntry_isDualBand(entry)) {
        dbgf(estimatorState.dbgModule, DBGDUMP,
             "%s: Ignoring stats for non-dual band STA " lbMACAddFmt(":"),
             __func__, lbMACAddData(addr->ether_addr_octet));
        return;
    }

    stadbEntry_bssStatsHandle_t bssStats = stadbEntry_getServingBSS(entry, NULL);
    if (!bssStats) {
        // Not actually associated
        dbgf(estimatorState.dbgModule, DBGDEBUG,
             "%s: Ignoring stats for disassociated STA " lbMACAddFmt(":"),
             __func__, lbMACAddData(addr->ether_addr_octet));
        return;
    }

    const lbd_bssInfo_t *assocBSS = stadbEntry_resolveBSSInfo(bssStats);
    lbDbgAssertExit(estimatorState.dbgModule, assocBSS);

    if (!lbAreBSSesSame(assocBSS, bss)) {
        dbgf(estimatorState.dbgModule, DBGDEBUG,
             "%s: Ignoring stats for STA " lbMACAddFmt(":")
             " on BSS " lbBSSInfoAddFmt() " because associated BSS is "
             lbBSSInfoAddFmt(),
             __func__, lbMACAddData(addr->ether_addr_octet),
             lbBSSInfoAddData(bss), lbBSSInfoAddData(assocBSS));
        return;
    }

    if (stadbEntry_setUplinkRSSI(entry, bssStats, stats->rssi,
                                 LBD_FALSE /* estimated */) == LBD_NOK) {
        dbgf(estimatorState.dbgModule, DBGERR,
             "%s: Failed to record RSSI (%u) for STA " lbMACAddFmt(":")
             " on BSS " lbBSSInfoAddFmt(),
             __func__, stats->rssi, lbMACAddData(addr->ether_addr_octet),
             lbBSSInfoAddData(assocBSS));
        return;
    }

    wlanif_band_e servingBand = wlanif_resolveBandFromFreq(assocBSS->freq);
    lbDbgAssertExit(estimatorState.dbgModule, servingBand != wlanif_band_invalid);

    if (estimatorState.config.interferenceDetectionEnable[servingBand]) {
        // These are stats we are interested in
        estimatorCmnHandleSTAStats(entry, addr, bssStats, assocBSS, stats);
    }
}

/**
 * @brief Compute the number of microseconds that have elapsed between two
 *        time samples.
 *
 * @note This function assumes the two samples are close enough together
 *       that the computation will not experience an integer overflow.
 *
 * @param [in] start  the beginning timestamp
 * @param [in] end  the ending timestamp
 *
 * @return the elapsed microseconds
 */
u_int32_t estimatorCmnComputeTimeDiff(
        const struct timespec *start, const struct timespec *end) {
#define NSECS_PER_USEC 1000
#define NSECS_PER_SEC 1000000000

    u_int32_t elapsedUsec = (end->tv_sec - start->tv_sec) * USECS_PER_SEC;

    long endNsec = end->tv_nsec;
    if (endNsec < start->tv_nsec) {
        // If the nanoseconds wrapped around, the number of seconds must
        // also have advanced by at least 1. Account for this by moving
        // one second worth of time from the elapsed microseconds into the
        // ending nanoseconds so that the subtraction below will always
        // result in a positive number.
        elapsedUsec -= USECS_PER_SEC;
        endNsec += NSECS_PER_SEC;
    }

    elapsedUsec += ((endNsec - start->tv_nsec) / NSECS_PER_USEC);
    return elapsedUsec;

#undef NSECS_PER_SEC
#undef NSECS_PER_USEC
}

lbd_airtime_t estimatorCmnComputeAirtime(u_int32_t dlThroughput, u_int32_t ulThroughput,
                                         wlanif_band_e band,
                                         lbd_linkCapacity_t linkRate) {
    // The link rate we have here is a PHY rate. To better represent an
    // upper layer rate (without MAC overheads), we apply a scaling factor.
    lbd_linkCapacity_t scaledLinkRate = estimator_estimateTCPFullCapacity(band, linkRate);

    // Either we got an invalid rate from the driver or the rate is so low
    // that when we scale it and perform integer division, it ends up being
    // 0. In either case, we avoid computing an airtime since we cannot
    // really determine what the value should actually be.
    if (0 == scaledLinkRate) {
        return LBD_INVALID_AIRTIME;
    }

    // Note that we multiply by 100 here so that we do not need to involve
    // floating point division. Integral division with truncation should
    // provide sufficient accuracy.
    //
    // A 32-bit number is being used here to account for scenarios where
    // the airtime might come out much larger than 100%. This generally
    // only will happen for the non-serving channel, but in such a situation,
    // it could overflow an 8-bit integer.
    u_int32_t rawAirtime =
        (dlThroughput + ulThroughput) * MAX_PERCENT / scaledLinkRate;


    // In case our link rate is not representative or we are estimating
    // a non-serving channel where there is no chance to support the
    // throughput, we should saturate the airtime at 100%.
    lbd_airtime_t airtime;
    if (rawAirtime > MAX_PERCENT) {
        airtime = MAX_PERCENT;
    } else {
        airtime = rawAirtime;
    }

    return airtime;
}

/**
 * @brief Compute the airtime, throughput, and capacity values and store them
 *        back in the entry.
 *
 * @param [in] entry  the entry to update
 * @param [in] addr  the address of the STA
 * @param [in] params  the state information for this entry
 * @param [in] endTime  the time at which the ending stats were sampled
 * @param [in] endStats  the ending stats snapshot
 *
 * @return LBD_OK on success; otherwise LBD_NOK
 */
static LBD_STATUS estimatorCmnComputeAndStoreServingStats(
        stadbEntry_handle_t entry,
        const struct ether_addr *addr,
        estimatorSTAState_t *state,
        const struct timespec *endTime,
        const wlanif_staStatsSnapshot_t *endStats) {
    stadbEntry_bssStatsHandle_t bssStats =
        stadbEntry_getServingBSS(entry, NULL);

    // First sanity check that the currently serving BSS is the one on which
    // the measurements were taken.
    if (!bssStats ||
        !lbAreBSSesSame(&state->statsEnabledBSSInfo,
                        stadbEntry_resolveBSSInfo(bssStats))) {
        dbgf(estimatorState.dbgModule, DBGERR,
             "%s: BSS " lbBSSInfoAddFmt() " used to measure stats for "
             lbMACAddFmt(":") " is no longer serving",
             __func__, lbBSSInfoAddData((&state->statsEnabledBSSInfo)),
             lbMACAddData(addr->ether_addr_octet));
        return LBD_NOK;
    }

    wlanif_band_e band =
        wlanif_resolveBandFromFreq(state->statsEnabledBSSInfo.freq);

    u_int64_t deltaBitsTx =
        (endStats->txBytes - state->lastStatsSnapshot.txBytes) * 8;
    u_int64_t deltaBitsRx =
        (endStats->rxBytes - state->lastStatsSnapshot.rxBytes) * 8;

    LBD_STATUS result;
    do {
        u_int32_t elapsedUsec =
            estimatorCmnComputeTimeDiff(&state->lastSampleTime, endTime);

        // Should never really happen, but if the elapsed time is 0,
        // abort the whole metrics storage. Something strange must have
        // happened with the clock.
        if (0 == elapsedUsec) {
            dbgf(estimatorState.dbgModule, DBGERR,
                 "%s: No time elapsed between samples for " lbMACAddFmt(":")
                 "; cannot estimate throughputs and airtime",
                 __func__, lbMACAddData(addr->ether_addr_octet));
            result = LBD_NOK;
            break;
        }

        result = stadbEntry_setFullCapacities(entry, bssStats,
                                              endStats->lastRxRate /*ulCapacity*/,
                                              endStats->lastTxRate /*dlCapacity*/);
        if (result != LBD_OK) { break; }

        lbd_linkCapacity_t dlThroughput = deltaBitsTx / elapsedUsec;
        lbd_linkCapacity_t ulThroughput = deltaBitsRx / elapsedUsec;
        result = stadbEntry_setLastDataRate(entry, dlThroughput, ulThroughput);
        if (result != LBD_OK) { break; }

        lbd_airtime_t airtime =
            estimatorCmnComputeAirtime(dlThroughput, ulThroughput,
                                       band, endStats->lastTxRate);
        if (LBD_INVALID_AIRTIME == airtime) {
            dbgf(estimatorState.dbgModule, DBGERR,
                 "%s: Unable to compute airtime for " lbMACAddFmt(":")
                 ": DL: %u Mbps, UL: %u Mbps, Link rate: %u Mbps",
                 __func__, lbMACAddData(addr->ether_addr_octet),
                 dlThroughput, ulThroughput, endStats->lastTxRate);

            // Write the airtime back as invalid so that we are not relying on
            // a value that is out of sync with the throughput.
            //
            // Note that the return value here is ignored since we've already
            // printed an error and there is not much else we can do.
            stadbEntry_setAirtime(entry, bssStats, LBD_INVALID_AIRTIME);

            result = LBD_NOK;
            break;
        }

        result = stadbEntry_setAirtime(entry, bssStats, airtime);
        if (result == LBD_OK) {
            // In order to not fill up the logs when continuous throughput is
            // enabled, only log them at DUMP level.
            struct dbgModule *logModule =
                (state->measurementMode ==
                 estimatorMeasurementMode_throughputOnly ||
                 state->measurementMode ==
                 estimatorMeasurementMode_remoteThroughput) ?
                estimatorState.statsDbgModule : estimatorState.dbgModule;
            dbgf(logModule, DBGINFO,
                 "%s: Estimates for " lbMACAddFmt(":") " on " lbBSSInfoAddFmt()
                 ": DL: %u Mbps, UL: %u Mbps, Link rate: %u Mbps, Airtime %u%%",
                 __func__, lbMACAddData(addr->ether_addr_octet),
                 lbBSSInfoAddData((&state->statsEnabledBSSInfo)),
                 dlThroughput, ulThroughput, endStats->lastTxRate, airtime);

            estimatorCmnDiaglogServingStats(addr, &state->statsEnabledBSSInfo,
                                            dlThroughput, ulThroughput,
                                            endStats->lastTxRate, airtime);
        }
    } while (0);

    return result;
}

LBD_STATUS estimatorCmnComputeAndStoreStats(stadbEntry_handle_t entry,
                                            const struct ether_addr *addr, u_int8_t flags,
                                            stadbEntry_bssStatsHandle_t targetBSSStats,
                                            lbd_linkCapacity_t capacity,
                                            lbd_rcpi_t rcpi) {
    LBD_STATUS result;
    const lbd_bssInfo_t *bssInfo = stadbEntry_resolveBSSInfo(targetBSSStats);
    lbDbgAssertExit(estimatorState.dbgModule, bssInfo);

    wlanif_band_e band = wlanif_resolveBandFromFreq(bssInfo->freq);

    do {
        // This can be called for a serving band estimate on the same
        // AP in which case no valid RCPI is available.
        if (rcpi != LBD_INVALID_RCPI) {
            result = stadbEntry_setRCPI(entry, targetBSSStats, rcpi);
            if (result != LBD_OK) { break; }
        }

        result = stadbEntry_setFullCapacities(entry, targetBSSStats,
                LBD_UNKNOWN_LINK_CAP, capacity);
        if (result != LBD_OK) { break; }

        lbd_airtime_t airtime = LBD_INVALID_AIRTIME;
        if (flags & estimatorNonServingRateAirtimeFlags_airtime) {
            lbd_linkCapacity_t dlThroughput, ulThroughput;
            result = stadbEntry_getLastDataRate(entry, &dlThroughput,
                                                &ulThroughput, NULL);
            if (result != LBD_OK) { break; }

            airtime =
                estimatorCmnComputeAirtime(dlThroughput, ulThroughput, band, capacity);
            result = stadbEntry_setAirtime(entry, targetBSSStats, airtime);
        }

        if (result == LBD_OK) {
            const lbd_bssInfo_t *targetBSS =
                stadbEntry_resolveBSSInfo(targetBSSStats);
            lbDbgAssertExit(estimatorState.dbgModule, targetBSS);

            dbgf(estimatorState.dbgModule, DBGINFO,
                 "%s: Estimates for " lbMACAddFmt(":") " on " lbBSSInfoAddFmt()
                 ": Link rate: %u Mbps, Airtime %u%%",
                 __func__, lbMACAddData(addr->ether_addr_octet),
                 lbBSSInfoAddData(targetBSS), capacity, airtime);

            estimatorCmnDiaglogNonServingStats(addr, targetBSS, capacity, airtime);
        }
    } while (0);

    return result;
}

/**
 * @brief Determine the channel to perform an 802.11k beacon report on and
 *        then request it.
 *
 * This will also start the 802.11k timer. Note that if no channel is
 * available, an error will be reported and the state will go back to idle.
 *
 * @param [in] entry  the STA for which to perform the measurement
 * @param [in] addr  the MAC address of the STA
 * @param [in] state  the current internal state of the STA
 *
 * @return LBD_OK on success; otherwise LBD_NOK
 */
static LBD_STATUS estimatorCmnPerform11kMeasurement(stadbEntry_handle_t entry,
                                                    const struct ether_addr *addr,
                                                    estimatorSTAState_t *state) {
    if (stadbEntry_getActStatus(entry, &state->activeBefore11k, NULL) == LBD_OK) {
        uint8_t maxNumSelectedChannels = 8;
        lbd_channelId_t dot11kChannel[8];
        uint16_t dot11kFreq[8];
        if (LBD_OK == steeralg_select11kChannel(entry, state->trigger, &maxNumSelectedChannels, dot11kChannel, dot11kFreq)) {
            stadbEntry_bssStatsHandle_t bssStats = stadbEntry_getServingBSS(entry, NULL);
            const lbd_bssInfo_t *servingBSS = stadbEntry_resolveBSSInfo(bssStats);
            lbDbgAssertExit(estimatorState.dbgModule, servingBSS);
            stadbEntry_resetBeaconReport(entry);
            stadbEntry_setPendingBeaconReport(entry, maxNumSelectedChannels);
            stadbEntry_setMaxNumSelectedChannels(entry, maxNumSelectedChannels);
            stadbEntry_setBeaconReportChannelsRequested(entry, dot11kChannel, dot11kFreq);
            if (estimatorRequestDownlinkRSSI(entry, bssStats, servingBSS,
                                             1 /* numChannels */, dot11kChannel, dot11kFreq,
                                             LBD_FALSE /* useBeaconTable */) == LBD_OK) {
                estimatorCmnStart11kResponseTimeout(state);
                dbgf(estimatorState.dbgModule, DBGINFO,
                     "%s: Initiated 11k measurement for "
                     lbMACAddFmt(":") " on channel %u Max number of channels : %u  from serving BSS "
                     lbBSSInfoAddFmt(), __func__,
                     lbMACAddData(addr->ether_addr_octet), dot11kChannel[0], maxNumSelectedChannels,
                     lbBSSInfoAddData(servingBSS));

                if (maxNumSelectedChannels > 1) {
                    stadbEntry_setPendingBeaconReportRequest(entry, 1);
                    estimatorCmnStart11kRequestTimeout(state);
                }

                return LBD_OK;
            } else {
                dbgf(estimatorState.dbgModule, DBGERR,
                     "%s: Failed to initiate 11k measurement for "
                     lbMACAddFmt(":") " on channel %u from serving BSS "
                     lbBSSInfoAddFmt(), __func__,
                     lbMACAddData(addr->ether_addr_octet), dot11kChannel[0],
                     lbBSSInfoAddData(servingBSS));
            }
        } else {
            dbgf(estimatorState.dbgModule, DBGERR,
                 "%s: No available channel for 11k measurement for "
                 lbMACAddFmt(":"), __func__,
                 lbMACAddData(addr->ether_addr_octet));
        }
    } else {
        dbgf(estimatorState.dbgModule, DBGERR,
             "%s: Failed to initiate 11k measuremrent for "
             lbMACAddFmt(":") " due to unable to get activity status",
             __func__, lbMACAddData(addr->ether_addr_octet));
    }

    state->dot11kState = estimator11kState_idle;
    return LBD_NOK;
}

/**
 * @brief Update the state of the per-STA airtime estimation process.
 *
 * If all STAs that were having their airtime measured are complete, this
 * will generate the event indicating the number of successful measurements.
 *
 * @param [in] state  the internal state tracking this entry
 * @param [in] isFailure  whether the completion is due to a failure
 */
static void estimatorCmnCompletePerSTAAirtime(estimatorSTAState_t *state,
                                           LBD_BOOL isFailure) {
    if (estimatorMeasurementMode_airtimeOnChannel == state->measurementMode) {
        if (!isFailure) {
            estimatorState.airtimeOnChannelState.numSTAsSuccess++;
        } else {
            estimatorState.airtimeOnChannelState.numSTAsFailure++;
        }

        estimatorState.airtimeOnChannelState.numSTAsRemaining--;
        if (0 == estimatorState.airtimeOnChannelState.numSTAsRemaining) {
            estimatorCmnGeneratePerSTAAirtimeCompleteEvent(
                    estimatorState.airtimeOnChannelState.apId,
                    estimatorState.airtimeOnChannelState.channelId,
                    estimatorState.airtimeOnChannelState.numSTAsSuccess,
                    estimatorState.airtimeOnChannelState.freq);

            dbgf(estimatorState.dbgModule, DBGINFO,
                 "%s: Completed airtime on channel %u (success=%zu, fail=%zu)",
                 __func__, estimatorState.airtimeOnChannelState.channelId,
                 estimatorState.airtimeOnChannelState.numSTAsSuccess,
                 estimatorState.airtimeOnChannelState.numSTAsFailure);

            estimatorState.airtimeOnChannelState.channelId = LBD_CHANNEL_INVALID;
            estimatorState.airtimeOnChannelState.freq = LBD_FREQ_INVALID;
        }
    }
}

void estimatorLogSTAStats(estimatorSTAState_t *state, const struct ether_addr *addr,
                          const wlanif_staStatsSnapshot_t *stats) {
    struct dbgModule *logModule =
        (state->measurementMode == estimatorMeasurementMode_throughputOnly ||
         state->measurementMode == estimatorMeasurementMode_remoteThroughput) ?
        estimatorState.statsDbgModule : estimatorState.dbgModule;
    dbgf(logModule, DBGDUMP,
         "%s: Stats for " lbMACAddFmt(":") " on BSS " lbBSSInfoAddFmt()
         " for state %u: Tx bytes: %"PRIu64", Rx bytes: %"PRIu64", "
         "Tx rate: %u Mbps, Rx rate: %u Mbps",
         __func__, lbMACAddData(addr->ether_addr_octet),
         lbBSSInfoAddData((&state->statsEnabledBSSInfo)),
         state->throughputState,
         stats->txBytes, stats->rxBytes, stats->lastTxRate, stats->lastRxRate);
}

void estimatorCmnDiaglogServingStats(const struct ether_addr *addr,
                                     const lbd_bssInfo_t *bssInfo,
                                     lbd_linkCapacity_t dlThroughput,
                                     lbd_linkCapacity_t ulThroughput,
                                     lbd_linkCapacity_t lastTxRate,
                                     lbd_airtime_t airtime) {
    if (diaglog_startEntry(mdModuleID_Estimator,
                           estimator_msgId_servingDataMetrics,
                           diaglog_level_info)) {
        diaglog_writeMAC(addr);
        diaglog_writeBSSInfo(bssInfo);
        diaglog_write32(dlThroughput);
        diaglog_write32(ulThroughput);
        diaglog_write32(lastTxRate);
        diaglog_write8(airtime);
        diaglog_finishEntry();
    }
}

/**
 * @brief Send the estimated statistics for the non-serving BSS for a specific
 *        STA out to diagnostic logging.
 *
 * @param [in] addr  the MAC address of the STA
 * @param [in] bssInfo  the BSS on which the STA is associated
 * @param [in] capacity  the estimated full capacity for the STA on the BSS
 * @param [in] airtime  the estimated airtime (as computed from the
 *                      throughputs and rate)
 */
static void estimatorCmnDiaglogNonServingStats(const struct ether_addr *addr,
                                               const lbd_bssInfo_t *bssInfo,
                                               lbd_linkCapacity_t capacity,
                                               lbd_airtime_t airtime) {
    if (diaglog_startEntry(mdModuleID_Estimator,
                           estimator_msgId_nonServingDataMetrics,
                           diaglog_level_info)) {
        diaglog_writeMAC(addr);
        diaglog_writeBSSInfo(bssInfo);
        diaglog_write32(capacity);
        diaglog_write8(airtime);
        diaglog_finishEntry();
    }
}

/**
 * @brief Send the log indicating whether interference is detected for
 *        a given STA.
 *
 * @param [in] addr  the MAC address of the STA
 * @param [in] bssInfo  the BSS on which the STA is associated
 * @param [in] detected  whether interference is considered present or not
 */
static void estimatorCmnDiaglogSTAInterferenceDetected(
        const struct ether_addr *addr, const lbd_bssInfo_t *bssInfo,
        LBD_BOOL detected) {
    if (diaglog_startEntry(mdModuleID_Estimator,
                           estimator_msgId_staInterferenceDetected,
                           diaglog_level_debug)) {
        diaglog_writeMAC(addr);
        diaglog_writeBSSInfo(bssInfo);
        diaglog_write8(detected);
        diaglog_finishEntry();
    }
}

/**
 * @brief Send the diagnostic log about statistics captured to make interference
 *        detection decision
 *
 * @param [in] addr  the MAC address of the STA
 * @param [in] bssInfo  the BSS on which the stats are measured
 * @param [in] rssi  RSSI reported
 * @param [in] txRate  TX rate reported
 */
static void estimatorCmnDiaglogIASStats(
        const struct ether_addr *addr, const lbd_bssInfo_t *bssInfo,
        lbd_rssi_t rssi, lbd_linkCapacity_t txRate,
        u_int64_t byteCountDelta, u_int32_t packetCountDelta) {
    if (diaglog_startEntry(mdModuleID_Estimator,
                           estimator_msgId_iasSTAStats,
                           diaglog_level_debug)) {
        diaglog_writeMAC(addr);
        diaglog_writeBSSInfo(bssInfo);
        diaglog_write8(rssi);
        diaglog_write16(txRate);
        diaglog_write64(byteCountDelta);
        diaglog_write32(packetCountDelta);
        diaglog_finishEntry();
    }
}

/**
 * @brief Determine if the STA is in one of the 802.11k states where further
 *        full metrics measurements are not permitted.
 *
 * @param [in] state  the state object to check
 *
 * @return LBD_TRUE if the an 802.11k action is in progress; otherwise LBD_FALSE
 */
static inline LBD_BOOL estimatorCmnStateIs11kNotAllowed(const estimatorSTAState_t *state) {
    return state->dot11kState != estimator11kState_idle;
}

/**
 * @brief Compute capacity and airtime information for the STA on the
 *        serving and optionally also the non-serving channels.
 *
 * This will result in the entry's serving channel full capacity, last data rate
 * and airtime information being updated. On the non-serving channel (if
 * requested), an estimated capacity and airtime will be stored. In the case
 * where both serving and non-serving are being estimated, once both of these
 * are complete, an estimator_event_staDataMetricsComplete will be generated.
 * If only the serving channel is being estimated, once the estimate is done,
 * an estimator_perSTAAirtimeCompleteEvent will be generated.
 *
 * @param [in] handle  the handle of the STA for which to perform the estimate
 * @param [in] measurementMode  the type of measurement being undertaken
 * @param [in] trigger  the trigger of capacity and airtime estimation
 *
 * @return LBD_OK on success; otherwise LBD_NOK
 */
static LBD_STATUS estimatorCmnEstimateSTADataMetricsImpl(
        stadbEntry_handle_t handle, estimatorMeasurementMode_e measurementMode,
        steerexec_reason_e trigger) {
    const struct ether_addr *addr = stadbEntry_getAddr(handle);
    lbDbgAssertExit(estimatorState.dbgModule, addr);

    // Verify the trigger is valid
    switch (trigger) {
        case steerexec_reason_user:
        case steerexec_reason_activeUpgrade:
        case steerexec_reason_activeDowngradeRate:
        case steerexec_reason_activeDowngradeRSSI:
        case steerexec_reason_idleAPSteering:
        case steerexec_reason_activeAPSteering:
        case steerexec_reason_activeOffload:
        case steerexec_reason_interferenceAvoidance:
        case steerexec_reason_activeLegacyAPSteering:
            // Above are valid triggers for now
            break;
        default:
            dbgf(estimatorState.dbgModule, DBGERR,
                 "%s: Invalid data metrics measurement trigger (%u) for "
                 lbMACAddFmt(":"), __func__, trigger,
                 lbMACAddData(addr->ether_addr_octet));
            return LBD_NOK;
    }

    // Backhaul STAs cannot have their metrics estimated in this manner.
    if (stadbEntry_isBackhaulSTA(handle)) {
        return LBD_NOK;
    }

    // STA is expected to be associated at this point, but its
    // state could have changed
    stadbEntry_bssStatsHandle_t bssStats;
    const lbd_bssInfo_t *bssInfo;
    LBD_BOOL selfServing = estimatorCmnIsSelfServing(handle, &bssStats, &bssInfo);
    if (!bssStats) {
        dbgf(estimatorState.dbgModule, DBGERR,
             "%s: STA " lbMACAddFmt(":") " not associated", __func__,
             lbMACAddData(addr->ether_addr_octet));
        return LBD_NOK;
    }

    estimatorSTAState_t *state = estimatorCmnGetOrCreateSTAState(handle);
    if (!state) {
        dbgf(estimatorState.dbgModule, DBGERR,
             "%s: Failed to allocate estimator state for "
             lbMACAddFmt(":"), __func__,
             lbMACAddData(addr->ether_addr_octet));
        return LBD_NOK;
    }

    // When STA is either doing an 11k measurement or has the 11k prohibit
    // timer running, we do not permit a new measurement if we anticipate it
    // will need 11k immediately or shortly thereafter.
    if ((estimatorMeasurementMode_full == measurementMode ||
         estimatorMeasurementMode_airtimeOnChannel == measurementMode) &&
        (estimatorMeasurementMode_full == state->measurementMode ||
         estimatorCmnStateIs11kNotAllowed(state))) {

        dbgf(estimatorState.dbgModule, DBGINFO,
             "%s: Cannot perform estimate for "
             lbMACAddFmt(":") " as 11k estimate is in progress (state %u)",
             __func__, lbMACAddData(addr->ether_addr_octet),
             state->dot11kState);
        return LBD_NOK;
    }

    // Check steering attempts of STA after checking 11k prohibit timer
    if (steerexec_isSubjectToSteeringLimit(trigger)) {
        if (stadbEntry_incSteerAttemptCount(handle) != LBD_OK) {
            dbgf(
                estimatorState.dbgModule, DBGERR,
                "%s: Failed to increase steering attempt count for STA " lbMACAddFmt(":"),
                __func__, lbMACAddData(addr->ether_addr_octet));
            return LBD_NOK;
        }

        // Ignore return status, as the entry has been checked when inc the counter
        u_int8_t steerCnt = 0;
        stadbEntry_getSteerAttemptCount(handle, &steerCnt);

        if (steerCnt > estimatorState.config.apSteerMaxRetryCount) {
            dbgf(
                estimatorState.dbgModule, DBGINFO,
                "%s: Steering attempts (%u) exceeds limit (%u) for STA " lbMACAddFmt(":"),
                __func__, steerCnt, estimatorState.config.apSteerMaxRetryCount,
                lbMACAddData(addr->ether_addr_octet));
            return LBD_NOK;
        }
    }

    if (estimatorMeasurementMode_full == measurementMode &&
        (!selfServing ||
         estimatorMeasurementMode_remoteThroughput != state->measurementMode)) {
        if (estimatorCmnStateIsSampling(state)) {
            // Upgrade the measurement to a full measurement.
            dbgf(estimatorState.dbgModule, DBGINFO,
                 "%s: Upgrading to full metrics for " lbMACAddFmt(":"),
                 __func__, lbMACAddData(addr->ether_addr_octet));

            if (estimatorMeasurementMode_airtimeOnChannel == state->measurementMode) {
                // An upgrade to full metrics should be considered a failure
                // since we do not want steeralg to consider it for offloading.
                estimatorCmnCompletePerSTAAirtime(state, LBD_TRUE /* isFailure */);
            }

            state->measurementMode = measurementMode;
            state->trigger = trigger;
            return LBD_OK;
        } else if (estimatorCmnCanSkipServingStats(handle, bssStats, trigger)) {
            // No need to re-measure. Proceed immediately to 802.11k.
            state->trigger = trigger;
            return estimatorCmnPerform11kMeasurement(handle, addr, state);
        }
    }

    // There are three cases where we need to enable the stats:
    // 1. No stats are being measured for the STA at all.
    // 2. Throughput was being measured remotely and now we want to measure
    //    it locally.
    // 3. To trigger STA stats before measuring the non serving metrics in legacy AP
    //    steering cases
    if (estimatorMeasurementMode_none == state->measurementMode ||
        estimatorMeasurementMode_remoteThroughput == state->measurementMode ||
        estimatorMeasurementMode_staStatsOnly == measurementMode) {
        // First enable the stats sampling on the radio. This will be a nop
        // if they are already enabled.
        if (selfServing) {
            dbgf(estimatorState.dbgModule, DBGDEBUG,
                 "%s: Enabling stats on " lbBSSInfoAddFmt(), __func__,
                 lbBSSInfoAddData(bssInfo));
            if (wlanif_enableSTAStats(bssInfo) != LBD_OK) {
                // wlanif should have already printed an error message.
                return LBD_NOK;
            }
        }
    }

    // Note that we are potentially resetting ourselves back to waiting for
    // the first sample. This is done to ensure that if we are doing an
    // airtime on channel estimate, all samples are performed at the same
    // time.
    state->measurementMode = measurementMode;
    state->throughputState = estimatorThroughputState_awaitingFirstSample;
    state->trigger = trigger;

    lbCopyBSSInfo(bssInfo, &state->statsEnabledBSSInfo);

    // Only start stats timer for locally associated STAs
    if (selfServing) {
        // If the timer is not already running, start it, but only if this
        // is not a continuous throughput estimate (since in that case the
        // timer always runs).
        if (estimatorMeasurementMode_throughputOnly != state->measurementMode) {
            unsigned secsRemaining, usecsRemaining;
            if (evloopTimeoutRemaining(&estimatorState.statsSampleTimer, &secsRemaining,
                                       &usecsRemaining)) {
                evloopTimeoutRegister(&estimatorState.statsSampleTimer,
                                      estimatorState.config.statsSampleInterval,
                                      0 /* usec */);
            }
        }
    }

    return LBD_OK;
}

/**
 * @brief Handler for each entry when checking whether to update the STA
 *        statistics.
 *
 * @param [in] entry  the current entry being examined
 * @param [in] cookie  the parameter provided in the stadb_iterate call
 */
static void estimatorSTAStatsSampleIterateCB(stadbEntry_handle_t entry,
                                             void *cookie) {
    estimatorSTAStatsSnapshotParams_t *params =
        (estimatorSTAStatsSnapshotParams_t *) cookie;
    lbDbgAssertExit(estimatorState.dbgModule, params);

    if (stadbEntry_isBackhaulSTA(entry)) {
        // Backhaul STAs are not sampled in the normal manner.
        return;
    }

    stadbEntry_bssStatsHandle_t servingBSS;
    const lbd_bssInfo_t *servingBSSInfo;
    LBD_BOOL selfServing = estimatorCmnIsSelfServing(entry, &servingBSS,
                                                     &servingBSSInfo);

    estimatorSTAState_t *state = stadbEntry_getEstimatorState(entry);

    // We only want to sample if the local AP is serving the STA or we're
    // in the sampling state and the last sample we took was on a local BSS.
    // In the latter case, we are doing the sample to ensure the sampling gets
    // completed.
    if (state && (selfServing || (estimatorCmnStateIsSampling(state) &&
                                  lbIsBSSLocal(&state->statsEnabledBSSInfo)))) {
        const struct ether_addr *addr = stadbEntry_getAddr(entry);
        lbDbgAssertExit(estimatorState.dbgModule, addr);

        // Only need to do a sample in one of two states.
        if (estimatorCmnStateIsFirstSample(state)) {
            // wlanif will check for a null BSS info and reject it.
            if (wlanif_sampleSTAStats(&state->statsEnabledBSSInfo, addr,
                                      LBD_FALSE /* rateOnly */,
                                      &state->lastStatsSnapshot) != LBD_OK) {
                estimatorCmnCompleteSTAStatsSample(entry, addr, state,
                                                   NULL, NULL, LBD_TRUE /* isFailure */);
            } else {
                lbGetTimestamp(&state->lastSampleTime);

                estimatorLogSTAStats(state, addr, &state->lastStatsSnapshot);

                state->throughputState = estimatorThroughputState_awaitingSecondSample;
                params->numPending++;
            }
        } else if (estimatorCmnStateIsSecondSample(state)) {
            wlanif_staStatsSnapshot_t stats;
            if (wlanif_sampleSTAStats(&state->statsEnabledBSSInfo, addr,
                                      LBD_FALSE /* rateOnly */,
                                      &stats) != LBD_OK) {
                estimatorCmnCompleteSTAStatsSample(entry, addr, state,
                                                   NULL, NULL, LBD_TRUE /* isFailure */);
            } else {
                struct timespec curTime;
                lbGetTimestamp(&curTime);

                estimatorLogSTAStats(state, addr, &stats);

                if (stats.txBytes >= state->lastStatsSnapshot.txBytes &&
                    stats.rxBytes >= state->lastStatsSnapshot.rxBytes) {
                    LBD_BOOL isFailure = LBD_FALSE;
                    if (estimatorCmnComputeAndStoreServingStats(
                                entry, addr, state, &curTime, &stats) != LBD_OK) {
                        isFailure = LBD_TRUE;
                    }

                    estimatorCmnCompleteSTAStatsSample(entry, addr, state,
                                                       &curTime, &stats, isFailure);
                } else {  // wraparound, so just set up for another sample
                    state->lastSampleTime = curTime;
                    state->lastStatsSnapshot = stats;
                    params->numPending++;
                }
            }
        } else if (estimatorState.config.enableContinuousThroughput) {
            lbDbgAssertExit(estimatorState.dbgModule, selfServing);

            // Restart the sampling.
            estimatorCmnEstimateSTADataMetricsImpl(
                entry, estimatorMeasurementMode_throughputOnly, state->trigger);
        }
    } else if (estimatorState.config.enableContinuousThroughput &&
               selfServing) {
        // Create the state and trigger the first sample.
        estimatorCmnEstimateSTADataMetricsImpl(
            entry, estimatorMeasurementMode_throughputOnly, steerexec_reason_user);
    }
}

/**
 * @brief React to an expiry of the stats timeout handler.
 *
 * @param [in] cookie  ignored
 */
static void estimatorCmnSTAStatsSampleTimeoutHandler(void *cookie) {
    estimatorSTAStatsSnapshotParams_t params = { 0 };
    if (stadb_iterate(estimatorSTAStatsSampleIterateCB, &params) != LBD_OK) {
        dbgf(estimatorState.dbgModule, DBGERR,
             "%s: Failed to iterate over STA DB; no updates will be done",
             __func__);
        return;
    }

    if (estimatorState.config.enableContinuousThroughput ||
        params.numPending > 0) {
        evloopTimeoutRegister(&estimatorState.statsSampleTimer,
                              estimatorState.config.statsSampleInterval,
                              0 /* usec */);
    }
}

/**
 * @brief Notify all registered oberservers that the provided entry can
 *        now have its data metrics measured again.
 *
 * @param [in] entry  the entry that was updated
 */
static void estimatorCmnNotifySTADataMetricsAllowedObservers(
        stadbEntry_handle_t entry) {
    size_t i;
    for (i = 0; i < MAX_STA_DATA_METRICS_ALLOWED_OBSERVERS; ++i) {
        struct estimatorSTADataMetricsAllowedObserver *curSlot =
            &estimatorState.staDataMetricsAllowedObservers[i];
        if (curSlot->isValid) {
            curSlot->callback(entry, curSlot->cookie);
        }
    }
}

/**
 * @brief Determine whether the 802.11k response timer has elapsed for this
 *        STA.
 *
 * @param [in] state  the state for the STA to check
 * @param [in] curTime  the current timestamp
 *
 * @return LBD_TRUE if the timer has expired; otherwise LBD_FALSE
 */
static LBD_BOOL estimatorIsDot11kResponseTimeout(
        const estimatorSTAState_t *state, const struct timespec *curTime) {
    if (state->dot11kState == estimator11kState_awaiting11kBeaconReport &&
            lbIsTimeAfter(curTime, &state->dot11kTimeout)) {
        return LBD_TRUE;
    }

    return LBD_FALSE;
}

/**
 * @brief Determine whether the 802.11k prohibit timer has elapsed for this
 *        STA.
 *
 * @param [in] state  the state for the STA to check
 * @param [in] curTime  the current timestamp
 *
 * @return LBD_TRUE if the timer has expired; otherwise LBD_FALSE
 */
static LBD_BOOL estimatorIsDot11kProhibitTimeout(
        const estimatorSTAState_t *state, const struct timespec *curTime) {
    if (state->dot11kState == estimator11kState_awaiting11kProhibitExpiry &&
            lbIsTimeAfter(curTime, &state->dot11kTimeout)) {
        return LBD_TRUE;
    }

    return LBD_FALSE;
}

/**
 * @brief Examine a single entry to see if its 802.11k timer has elapsed.
 *
 * @param [in] entry  the entry to examine
 * @param [in] cookie  not used here
 */
static void estimatorDot11kIterateCB(stadbEntry_handle_t entry, void *cookie) {
    estimatorSTAState_t *state = stadbEntry_getEstimatorState(entry);
    if (state) {
        struct timespec curTime;
        lbGetTimestamp(&curTime);

        if (estimatorIsDot11kResponseTimeout(state, &curTime)) {
            const struct ether_addr *addr = stadbEntry_getAddr(entry);
            lbDbgAssertExit(estimatorState.dbgModule, addr);

            dbgf(estimatorState.dbgModule, DBGERR,
                 "%s: Timeout waiting for 802.11k response from "
                 lbMACAddFmt(":"), __func__,
                 lbMACAddData(addr->ether_addr_octet));


            wlanif_beaconReport_t bcnrpt = {0};
            stadbEntryRetrieveBeaconReport(entry, &bcnrpt);
            dbgf(estimatorState.dbgModule, DBGDEBUG,
                 "%s %d: Beacon reports for STA " lbMACAddFmt(":") " numBcnRpt %zu",
                 __func__, __LINE__, lbMACAddData(bcnrpt.sta_addr.ether_addr_octet), bcnrpt.numBcnrpt);

            // In case of multi 11k request with fewer response from client in multi channel
            // react to available 11k response before timeout.
            if (bcnrpt.numBcnrpt) {
                const lbd_bssInfo_t *reportedLocalBss = NULL;
                lbd_channelId_t measuredChannel = LBD_CHANNEL_INVALID;
                u_int16_t measuredFreq = LBD_FREQ_INVALID;

                if (estimator11kState_awaiting11kBeaconReport == state->dot11kState) {
                    // No longer need the response timer.
                    estimatorState.numDot11kTimers--;

                    LBD_STATUS result = LBD_NOK;
                    do {
                        if (estimatorComputeServingBandRateAirtime(
                                    entry, state, &bcnrpt.sta_addr) != LBD_OK) {
                            result = LBD_NOK;
                            break;
                        }

                        result = estimatorCmnStoreBeaconReportEvent(
                            entry, &bcnrpt,
                            estimatorNonServingRateAirtimeFlags_airtime |
                                estimatorNonServingRateAirtimeFlags_nonMeasured,
                            &reportedLocalBss, &measuredChannel, &measuredFreq);
                    } while(0);

                    estimatorCmnCompleteDot11kMeasurement(state, &bcnrpt.sta_addr,
                                                          result, measuredChannel,measuredFreq,
                                                          reportedLocalBss,
                                                          LBD_TRUE /* startProhibitTimer */);
                }
            } else {
                // No prohibit timer will be started here, the timer will be
                // scheduled after iteration.
                estimatorCmnCompleteDot11kMeasurement(state, addr, LBD_NOK,
                                                      LBD_CHANNEL_INVALID,
                                                      LBD_FREQ_INVALID,
                                                      NULL /* reportedBSS */,
                                                      LBD_FALSE /* startProhibitTimer */);
            }
            if (lbIsTimeBefore(&state->dot11kTimeout,
                               &estimatorState.nextDot11kExpiry)) {
                estimatorState.nextDot11kExpiry = state->dot11kTimeout;
            }
        } else if (estimatorIsDot11kProhibitTimeout(state, &curTime)) {
            const struct ether_addr *addr = stadbEntry_getAddr(entry);
            lbDbgAssertExit(estimatorState.dbgModule, addr);

            dbgf(estimatorState.dbgModule, DBGINFO,
                 "%s: Prohibit timer expired for " lbMACAddFmt(":"),
                 __func__, lbMACAddData(addr->ether_addr_octet));
            state->dot11kState = estimator11kState_idle;

            estimatorState.numDot11kTimers--;

            // Note that this should be done last to make sure the state has
            // been reset to idle before the upcall. This will allow the
            // observer to trigger a new measurement if necessary.
            estimatorCmnNotifySTADataMetricsAllowedObservers(entry);
        } else if (estimatorCmnStateIs11kNotAllowed(state) &&
                   lbIsTimeBefore(&state->dot11kTimeout,
                                  &estimatorState.nextDot11kExpiry)) {
            estimatorState.nextDot11kExpiry = state->dot11kTimeout;
        }
    }
}

static LBD_BOOL estimatorIsDot11kRequestTimeout(
        const estimatorSTAState_t *state, const struct timespec *curTime) {
    if (state->dot11kState == estimator11kState_awaiting11kBeaconReport &&
            lbIsTimeAfter(curTime, &state->dot11kReqTimeout)) {
        return LBD_TRUE;
    }

    return LBD_FALSE;
}

static void estimatorDot11kReqIterateCB(stadbEntry_handle_t entry, void *cookie) {
    estimatorSTAState_t *state = stadbEntry_getEstimatorState(entry);
    if (state) {
        struct timespec curTime;
        lbGetTimestamp(&curTime);

        if (estimatorIsDot11kRequestTimeout(state, &curTime)) {
            const struct ether_addr *addr = stadbEntry_getAddr(entry);
            lbDbgAssertExit(estimatorState.dbgModule, addr);

            dbgf(estimatorState.dbgModule, DBGDEBUG,
                 "%s: Subsequent request for 802.11k response from "
                 lbMACAddFmt(":"), __func__,
                 lbMACAddData(addr->ether_addr_octet));

            lbd_channelId_t dot11kChannel[8];
            u_int16_t dot11kFreq[8];
            u_int8_t maxNumSelectedChannels =  0, pendingBcnRptReq = 0;

            stadbEntry_getMaxNumSelectedChannels(entry, &maxNumSelectedChannels);
            stadbEntry_getPendingBeaconReportRequest(entry, &pendingBcnRptReq);
            stadbEntry_getBeaconReportChannelsRequested(entry, dot11kChannel, dot11kFreq);
            stadbEntry_bssStatsHandle_t bssStats = stadbEntry_getServingBSS(entry, NULL);
            const lbd_bssInfo_t *servingBSS = stadbEntry_resolveBSSInfo(bssStats);
            lbDbgAssertExit(estimatorState.dbgModule, servingBSS);
            dbgf(estimatorState.dbgModule, DBGDEBUG, "%s: maxNumSelectedChannels %d, pendingBcnRptReq %d", __func__, maxNumSelectedChannels, pendingBcnRptReq);
            if (maxNumSelectedChannels) {
                if (pendingBcnRptReq < maxNumSelectedChannels) {
                    pendingBcnRptReq++;
                    if (estimatorRequestDownlinkRSSI(entry, bssStats, servingBSS,
                                                1 /* numChannels */, &dot11kChannel[pendingBcnRptReq-1],
                                                &dot11kFreq[pendingBcnRptReq-1],
                                                LBD_FALSE /* useBeaconTable */) == LBD_OK) {
                    }
                    stadbEntry_setPendingBeaconReportRequest(entry, pendingBcnRptReq);
                }
                if (pendingBcnRptReq >= maxNumSelectedChannels) {
                    if (estimatorState.numDot11kReqTimers >= 1) {
                        estimatorState.numDot11kReqTimers--;
                    }
                }
            } else {
                if (estimatorState.numDot11kReqTimers >= 1) {
                    estimatorState.numDot11kReqTimers--;
                }
            }

            if (lbIsTimeBefore(&state->dot11kReqTimeout,
                               &estimatorState.nextDot11kReqExpiry)) {
                estimatorState.nextDot11kReqExpiry = state->dot11kReqTimeout;
            }
        }
    }
}

/**
 * @brief React to an expiry of the timer for 802.11k operations.
 *
 * @param [in] cookie  ignored
 */
static void estimatorCmn11kTimeoutHandler(void *cookie) {
    struct timespec curTime = {0};
    lbGetTimestamp(&curTime);
    unsigned dot11kProhibitTimeLong = 0;
    u_int8_t index = 0;

    for (index = 0; index < BSTEERING_MAX_CLIENT_CLASS_GROUP; index++) {
        if (dot11kProhibitTimeLong <
            estimatorState.config.dot11kProhibitTimeLong[index]) {
            dot11kProhibitTimeLong =
                estimatorState.config.dot11kProhibitTimeLong[index];
        }
    }

    // This is the worst case. The iteration will adjust this based on the
    // actual devices that are still under prohibition or awaiting 11k
    // response.
    estimatorState.nextDot11kExpiry = curTime;
    estimatorState.nextDot11kExpiry.tv_sec += dot11kProhibitTimeLong;

    if (stadb_iterate(estimatorDot11kIterateCB, NULL) != LBD_OK) {
        dbgf(estimatorState.dbgModule, DBGERR,
             "%s: Failed to iterate over station database", __func__);

        // For now we are falling through to reschedule the timer.
    }

    if (estimatorState.numDot11kTimers != 0) {
        // + 1 here to make sure the timer expires after the deadline
        // (to avoid having to schedule a quick timer).
        evloopTimeoutRegister(&estimatorState.dot11kTimer,
                              estimatorState.nextDot11kExpiry.tv_sec + 1 -
                              curTime.tv_sec, 0);
    }
}

static void estimatorCmn11kReqTimeoutHandler(void *cookie) {
    struct timespec curTime = {0};
    lbGetTimestamp(&curTime);

    // This is the worst case. The iteration will adjust this based on the
    // actual devices that are still under prohibition or awaiting 11k
    // response.
    estimatorState.nextDot11kReqExpiry = curTime;
    estimatorState.nextDot11kReqExpiry.tv_sec += 1;

    if (stadb_iterate(estimatorDot11kReqIterateCB, NULL) != LBD_OK) {
        dbgf(estimatorState.dbgModule, DBGERR,
             "%s: Failed to iterate over station database", __func__);

        // For now we are falling through to reschedule the timer.
    }

    if (estimatorState.numDot11kReqTimers != 0) {
        // + 1 here to make sure the timer expires after the deadline
        // (to avoid having to schedule a quick timer).
        evloopTimeoutRegister(&estimatorState.dot11kReqTimer,
                              estimatorState.nextDot11kReqExpiry.tv_sec + 1 -
                              curTime.tv_sec, 0);
    }
}

LBD_BOOL estimatorCmnIsSelfServing(stadbEntry_handle_t entry,
                                   stadbEntry_bssStatsHandle_t *servingBSS,
                                   const lbd_bssInfo_t **servingBSSInfo) {
    if (!entry) {
        return LBD_FALSE;
    }

    *servingBSS = stadbEntry_getServingBSS(entry, NULL);
    if (!*servingBSS) {
        // Not associated; nothing to do.
        return LBD_FALSE;
    }

    *servingBSSInfo = stadbEntry_resolveBSSInfo(*servingBSS);
    lbDbgAssertExit(estimatorState.dbgModule, *servingBSSInfo);

    return lbIsBSSLocal(*servingBSSInfo);
}

LBD_BOOL estimatorCmnIsSameServingAP(stadbEntry_handle_t entry, lbd_apId_t apId,
                                     stadbEntry_bssStatsHandle_t *servingBSS,
                                     const lbd_bssInfo_t **servingBSSInfo) {
    if (!entry) {
        return LBD_FALSE;
    }

    *servingBSS = stadbEntry_getServingBSS(entry, NULL);
    if (!*servingBSS) {
        // Not associated; nothing to do.
        return LBD_FALSE;
    }

    *servingBSSInfo = stadbEntry_resolveBSSInfo(*servingBSS);
    lbDbgAssertExit(estimatorState.dbgModule, *servingBSSInfo);

    return (*servingBSSInfo)->apId == apId;
}

/**
 * @brief Handler for each entry when checking whether to measure the STA
 *        airtime on a particular channel.
 *
 * @param [in] entry  the current entry being examined
 * @param [inout] cookie  the parameter provided in the stadb_iterate call
 */
static void estimatorCmnStartSTAAirtimeIterateCB(stadbEntry_handle_t entry,
                                                 void *cookie) {
    estimatorPerSTAAirtimeOnChannelParams_t *params =
        (estimatorPerSTAAirtimeOnChannelParams_t *) cookie;
    lbDbgAssertExit(estimatorState.dbgModule, params);

    const struct ether_addr *addr = stadbEntry_getAddr(entry);
    lbDbgAssertExit(estimatorState.dbgModule, addr);

    if (stadbEntry_isBackhaulSTA(entry)) {
        dbgf(estimatorState.dbgModule, DBGDEBUG,
             "%s: Not measuring " lbMACAddFmt(":") " since it is a bSTA", __func__,
             lbMACAddData(addr->ether_addr_octet));
        return;
    }

    stadbEntry_bssStatsHandle_t servingBSS;
    const lbd_bssInfo_t *servingBSSInfo;
    if (!estimatorCmnIsSameServingAP(entry, params->apId, &servingBSS, &servingBSSInfo)) {
        dbgf(estimatorState.dbgModule, DBGDEBUG,
             "%s: Not measuring " lbMACAddFmt(":") " since it is not "
             "associated to the provided AP (%u)", __func__,
             lbMACAddData(addr->ether_addr_octet),
             params->apId);
        return;
    }

    if (servingBSSInfo->channelId != params->channelId ||
        stadbEntry_getReservedAirtime(entry, servingBSS) != LBD_INVALID_AIRTIME) {
        dbgf(estimatorState.dbgModule, DBGDEBUG,
             "%s: Not measuring " lbMACAddFmt(":") " due to channel mismatch "
             "(serving=%d, selected=%d) or airtime reservation", __func__,
             lbMACAddData(addr->ether_addr_octet),
             servingBSSInfo->channelId, params->channelId);
        return;
    }

    // Device must be active in order to have its airtime measured, regardless
    // of whether it is eligible for active steering or not.
    LBD_BOOL isActive = LBD_FALSE;
    if (stadbEntry_getActStatus(entry, &isActive, NULL /* age */) == LBD_NOK ||
        !isActive) {
        dbgf(estimatorState.dbgModule, DBGDEBUG,
             "%s: Not measuring " lbMACAddFmt(":") " due to not active",
             __func__, lbMACAddData(addr->ether_addr_octet));
        return;
    }

    // Only if the device is active steering eligible do we even consider it
    // for an airtime measurement (as when it is not, we either won't be able
    // to steer it because it is active or steering it won't reduce the
    // airtime because it is idle).
    if (steerexec_determineSteeringEligibility(entry) !=
            steerexec_steerEligibility_active) {
        dbgf(estimatorState.dbgModule, DBGDEBUG,
             "%s: Not measuring " lbMACAddFmt(":") " due to not active "
             "steering eligible", __func__, lbMACAddData(addr->ether_addr_octet));
        return;
    }

    // Attempt to start the airtime measurement. If this currently prohibited
    // (eg. due to it being done too recently ago), then we'll just skip this
    // entry.
    // Currently per channel airtime measurement is always triggered by offloading.
    if (estimatorCmnEstimateSTADataMetricsImpl(
            entry, estimatorMeasurementMode_airtimeOnChannel,
            steerexec_reason_activeOffload) == LBD_OK) {
        params->numSTAsStarted++;
    }
    // else, it probably is already running; an error message should have been
    // printed
}

/**
 * @brief Generate the event indicating that per-STA airtime estimates are
 *        complete on the provided channel.
 *
 * @param [in] apId  the AP on which the estimate was done
 * @param [in] channelId  the channel on which the estimate was done
 * @param [in] numSTAsEstimated  the number of STAs which had estimates
 *                               written
 */
static void estimatorCmnGeneratePerSTAAirtimeCompleteEvent(
    lbd_apId_t apId, lbd_channelId_t channelId, size_t numSTAsEstimated,
    u_int16_t freq) {
    estimator_perSTAAirtimeCompleteEvent_t event;
    event.apId = apId;
    event.channelId = channelId;
    event.freq = freq;
    event.numSTAsEstimated = numSTAsEstimated;

    mdCreateEvent(mdModuleID_Estimator, mdEventPriority_Low,
                  estimator_event_perSTAAirtimeComplete,
                  &event, sizeof(event));
}

/**
 * @brief Determine if the estimation step should skip throughput measurement and
 *        proceed directly to the 802.11k measurement
 *
 * Throughput measurement can be skipped if
 * 1. The client is idle, or
 * 2. The serving data rate and airtime information is recent.
 *
 * @param [in] entry  the STA to check
 * @param [in] servingBSSStats  the stats to check for airtime recency
 * @param [in] trigger  the trigger to this data metrics measurement
 *
 * @return LBD_TRUE if throughput measurement can be skipped;
 *         otherwise LBD_FALSE
 */
static LBD_BOOL estimatorCmnCanSkipServingStats(
        const stadbEntry_handle_t entry,
        const stadbEntry_bssStatsHandle_t servingBSSStats,
        steerexec_reason_e trigger) {
    if (trigger == steerexec_reason_idleAPSteering) {
        // If the client is idle, reset serving stats
        stadbEntry_setLastDataRate(entry, 0 /* dlThroughput */, 0 /* ulThroughput */);
        stadbEntry_setAirtime(entry, servingBSSStats, 0 /* airtime */);

        return LBD_TRUE;
    }

    lbd_linkCapacity_t dlRate, ulRate;
    time_t elapsedSecs;
    if (stadbEntry_getLastDataRate(entry, &dlRate, &ulRate,
                                   &elapsedSecs) == LBD_OK &&
            elapsedSecs <= MAX_SERVING_METRICS_AGE_SECS &&
            stadbEntry_getAirtime(entry, servingBSSStats, NULL) != LBD_INVALID_AIRTIME) {
        return LBD_TRUE;
    }

    return LBD_FALSE;
}

#ifdef LBD_DBG_MENU
static const char *estimatorThroughputEstimationStateStrs[] = {
    "Idle",
    "Awaiting 1st sample",
    "Awaiting 2nd sample",
    "Awaiting beacon report",
    "802.11k prohibited"
};

/**
 * @brief Obtain a string representation of the data rate estimation state.
 *
 * @param [in] state  the state for which to get the string
 *
 * @return  the string, or the empty string for an invalid state
 */
static const char *estimatorCmnGetThroughputEstimationStateStr(
        estimatorThroughputEstimationState_e state) {
    // Should not be possible unless a new state is introduced without
    // updating the array.
    lbDbgAssertExit(estimatorState.dbgModule,
                    state < sizeof(estimatorThroughputEstimationStateStrs) /
                    sizeof(estimatorThroughputEstimationStateStrs[0]));

    return estimatorThroughputEstimationStateStrs[state];
}

static const char *estimatorMeasModeStrs[] = {
    "None",
    "Full",
    "Airtime on channel",
    "Throughput only",
    "Remote throughput"
};

/**
 * @brief Obtain a string representation of the meas mode estimation state.
 *
 * @param [in] state  the state for which to get the string
 *
 * @return  the string, or the empty string for an invalid state
 */
static const char *estimatorCmnGetMeasModeStr(
        estimatorMeasurementMode_e state) {
    // Should not be possible unless a new state is introduced without
    // updating the array.
    lbDbgAssertExit(estimatorState.dbgModule,
                    state < sizeof(estimatorMeasModeStrs) /
                    sizeof(estimatorMeasModeStrs[0]));

    return estimatorMeasModeStrs[state];
}

static const char *estimator11kStateStrs[] = {
    "Idle",
    "Awaiting beacon report",
    "Independent beacon report",
    "802.11k prohibited"
};

/**
 * @brief Obtain a string representation of the data rate estimation state.
 *
 * @param [in] state  the state for which to get the string
 *
 * @return  the string, or the empty string for an invalid state
 */
static const char *estimatorCmnGet11kStateStr(estimator11kState_e state) {
    // Should not be possible unless a new state is introduced without
    // updating the array.
    lbDbgAssertExit(estimatorState.dbgModule,
                    state < sizeof(estimator11kStateStrs) /
                    sizeof(estimator11kStateStrs[0]));

    return estimator11kStateStrs[state];
}

// Help messages for estimator status command
static const char *estimatorMenuStatusHelp[] = {
    "s -- display status for all STAs",
    "Usage:",
    "\ts: dump status information",
    NULL
};

/**
 * @brief Parameters for dumping status when iterating over the station
 *        database.
 */
struct estimatorStatusCmdContext {
    struct cmdContext *context;
};

/**
 * @brief Dump the header for the STA status information.
 *
 * @param [in] context  the context handle to use for output
 */
static void estimatorStatusIterateCB(stadbEntry_handle_t entry, void *cookie) {
    struct estimatorStatusCmdContext *statusContext =
        (struct estimatorStatusCmdContext *) cookie;
    lbDbgAssertExit(estimatorState.dbgModule, statusContext);

    estimatorSTAState_t *state = stadbEntry_getEstimatorState(entry);
    if (state) {
        const struct ether_addr *addr = stadbEntry_getAddr(entry);
        lbDbgAssertExit(estimatorState.dbgModule, addr);

        u_int32_t remainingUsec = 0;

        // Only compute the 11k remaining time if it is actually running.
        if (estimator11kState_awaiting11kBeaconReport == state->dot11kState ||
            estimator11kState_awaiting11kProhibitExpiry == state->dot11kState) {
            struct timespec curTime = {0};
            lbGetTimestamp(&curTime);

            remainingUsec =
                estimatorCmnComputeTimeDiff(&curTime, &state->dot11kTimeout);
        }

        cmdf(statusContext->context, lbMACAddFmt(":") "  %-25s  %-25s  %-25s  %u.%u\n",
             lbMACAddData(addr->ether_addr_octet),
             estimatorCmnGetThroughputEstimationStateStr(state->throughputState),
             estimatorCmnGetMeasModeStr(state->measurementMode),
             estimatorCmnGet11kStateStr(state->dot11kState),
             remainingUsec / USECS_PER_SEC, remainingUsec % USECS_PER_SEC);
    }
}

#ifndef GMOCK_UNIT_TESTS
static
#endif
void estimatorMenuStatusHandler(struct cmdContext *context,
                                const char *cmd) {
    if (estimatorState.debugModeEnabled) {
        cmdf(context, "Debug mode is ON; interference detection "
                     "is not running\n\n");
    }

    struct estimatorStatusCmdContext statusContext = {
        context
    };

    cmdf(context, "%-17s  %-25s  %-25s  %-25s  %-12s\n",
         "MAC Address", "Throughput State", "Measurement Mode", "802.11k State",
         "802.11k Expiry (s)");
    if (stadb_iterate(estimatorStatusIterateCB, &statusContext) != LBD_OK) {
        cmdf(context, "Iteration over station database failed\n");
    }

    cmdf(context, "\n");
}

// Help messages for estimator rate command
static const char *estimatorMenuRateHelp[] = {
    "rate -- estimate rate for a STA",
    "Usage:",
    "\trate <mac addr>: estimate for the specified STA",
    NULL
};

#ifndef GMOCK_UNIT_TESTS
static
#endif
void estimatorMenuRateHandler(struct cmdContext *context,
                              const char *cmd) {
    char mac[CMD_MAC_ADDR_STR_LEN] = {0};

    if (!cmd) {
        cmdf(context, "estimator 'rate' command must include MAC address\n");
        return;
    }

    const char *arg = cmdWordFirst(cmd);
    cmdGetCurrArgNullTerm(arg, &mac[0], CMD_MAC_ADDR_STR_LEN);

    const struct ether_addr *staAddr = ether_aton(mac);
    if (!staAddr) {
        cmdf(context, "estimator 'rate' command invalid MAC address: %s\n",
             arg);
        return;
    }

    stadbEntry_handle_t entry = stadb_find(staAddr);
    if (!entry) {
        cmdf(context, "estimator 'rate' unknown MAC address: "
                      lbMACAddFmt(":") "\n",
             lbMACAddData(staAddr->ether_addr_octet));
        return;
    }
    stadbEntry_bssStatsHandle_t servingBSS = stadbEntry_getServingBSS(entry, NULL);
    if (!servingBSS) {
        cmdf(context, "estimator 'rate' STA not associated: " lbMACAddFmt(":") "\n",
             lbMACAddData(staAddr->ether_addr_octet));
        return;
    }

    if (estimator_estimateSTADataMetrics(entry, steerexec_reason_user) != LBD_OK) {
        cmdf(context, "estimator 'rate' " lbMACAddFmt(":")
                      " failed\n",
             lbMACAddData(staAddr->ether_addr_octet));
    }
}

// Help messages for estimator airtime command
static const char *estimatorMenuAirtimeHelp[] = {
    "airtime -- estimate airtime for all active STAs on a channel",
    "Usage:",
    "\tairtime <channel> <freq> [<ap id>]: estimate for the specified channel",
    NULL
};

#ifndef GMOCK_UNIT_TESTS
static
#endif
void estimatorMenuAirtimeHandler(struct cmdContext *context,
                                 const char *cmd) {
    if (!cmd) {
        cmdf(context,
             "estimator 'airtime' command must include channel\n");
        return;
    }

    const char *arg = cmdWordFirst(cmd);
    if (!cmdWordDigits(arg)) {
        cmdf(context, "Channel must be a decimal number\n");
        return;
    }

    lbd_channelId_t channel = atoi(arg);

    arg = cmdWordNext(arg);
    if (!cmdWordDigits(arg)) {
        cmdf(context, "estimator 'airtime' command invalid freq '%s'\n",
             arg);
        return;
    }

    u_int16_t freq = atoi(arg);
    if ( !( (freq >= WLANIF_6G_START_FREQ && freq <= WLANIF_6G_END_FREQ) ||
            (freq >= WLANIF_5G_START_FREQ && freq <= WLANIF_5G_END_FREQ) ||
            (freq >= WLANIF_2G_START_FREQ && freq <= WLANIF_2G_END_FREQ) ) ) {
        cmdf(context, "estimator 'airtime' command invalid freq '%s'\n",
             arg);
        return;
    }

    arg = cmdWordNext(arg);
    lbd_apId_t apId = LBD_APID_SELF;
    if (cmdWordDigits(arg)) {
        apId = atoi(arg);
    }

    if (estimator_estimatePerSTAAirtimeOnChannel(apId, channel, freq) != LBD_OK) {
        cmdf(context, "estimator 'airtime' %u failed\n", channel);
    }
}

// Help messages for estimator debug command
static const char *estimatorMenuDebugHelp[] = {
    "d -- enable/disable debug mode",
    "Usage:",
    "\td on: enable debug mode (ignoring interference detection events)",
    "\td off: disable debug mode (handling interference detection events)",
    NULL
};

#ifndef GMOCK_UNIT_TESTS
static
#endif
void estimatorMenuDebugHandler(struct cmdContext *context, const char *cmd) {
    LBD_BOOL isOn = LBD_FALSE;
    const char *arg = cmdWordFirst(cmd);

    if (!arg) {
        cmdf(context, "estimator 'd' command requires on/off argument\n");
        return;
    }

    if (cmdWordEq(arg, "on")) {
        isOn = LBD_TRUE;
    } else if (cmdWordEq(arg, "off")) {
        isOn = LBD_FALSE;
    } else {
        cmdf(context, "estimator 'd' command: invalid arg '%s'\n", arg);
        return;
    }

    dbgf(estimatorState.dbgModule, DBGINFO,
         "%s: Setting debug mode to %u", __func__, isOn);
    estimatorState.debugModeEnabled = isOn;
}

// Help messages for estimator detected command
static const char *estimatorMenuInterferenceDetectedHelp[] = {
    "detect -- record whether interference is detected for a given STA",
    "Usage:",
    "\tdetect <mac_addr> <0|1>: record value for the STA identified by MAC",
    NULL
};

#ifndef GMOCK_UNIT_TESTS
static
#endif
void estimatorMenuInterferenceDetectedHandler(struct cmdContext *context,
                                              const char *cmd) {

    char mac[CMD_MAC_ADDR_STR_LEN] = {0};

    if (!estimatorState.debugModeEnabled) {
        cmdf(context, "estimator 'detect' command is only valid when "
                      "debug mode is enabled\n");
        return;
    }

    if (!cmd) {
        cmdf(context, "estimator 'detect' command must include "
                      "MAC address\n");
        return;
    }

    const char *arg = cmdWordFirst(cmd);

    cmdGetCurrArgNullTerm(arg, &mac[0], CMD_MAC_ADDR_STR_LEN);
    const struct ether_addr *staAddr = ether_aton(mac);
    if (!staAddr) {
        cmdf(context, "estimator 'detect' command invalid MAC address: %s\n",
             arg);
        return;
    }

    stadbEntry_handle_t entry = stadb_find(staAddr);
    if (!entry) {
        cmdf(context, "estimator 'detect' unknown MAC address: "
                      lbMACAddFmt(":") "\n",
             lbMACAddData(staAddr->ether_addr_octet));
        return;
    }

    // Now determine the actual boolean value to assign.
    arg = cmdWordNext(arg);
    if (!cmdWordDigits(arg)) {
        cmdf(context, "estimator 'detect' requires numeric value for "
                      "interference detection state\n");
        return;
    }
    LBD_BOOL detected = (LBD_BOOL) atoi(arg);

    // Verify that the STA is actually associated on this AP.
    stadbEntry_bssStatsHandle_t servingBSS;
    const lbd_bssInfo_t *servingBSSInfo;
    if (!estimatorCmnIsSelfServing(entry, &servingBSS, &servingBSSInfo)) {
        // Invalid entry or not associated. Cannot measure the stats.
        cmdf(context, lbMACAddFmt(":") " is not associated to this AP; "
             "cannot add interference sample\n",
             lbMACAddData(staAddr->ether_addr_octet));
        return;
    }

    estimatorSTAState_t *state = estimatorCmnGetOrCreateSTAState(entry);
    if (!state) {
        dbgf(estimatorState.dbgModule, DBGERR,
             "%s: Failed to create state for " lbMACAddFmt(":"),
             __func__, lbMACAddData(staAddr->ether_addr_octet));
        return;
    }

    if (estimatorCmnAddInterferenceSample(entry, staAddr, servingBSS,
                                          servingBSSInfo, state, detected) != LBD_OK) {
        cmdf(context, "Failed to set interference detection state\n");
    }
}

// Help messages for estimator stats command
static const char *estimatorMenuInterferenceStatsHelp[] = {
    "stats -- inject stats to evaluate interference for a given STA",
    "Usage:",
    "\tstats <mac_addr> <rssi> <tx_rate> <tx_bytes> <tx_packets>: inject RSSI, "
    "TX rate, TX byte count and Tx packet count for the STA identified by MAC",
    NULL
};

#ifndef GMOCK_UNIT_TESTS
static
#endif
void estimatorMenuInterferenceStatsHandler(struct cmdContext *context,
                                           const char *cmd) {
    char mac[CMD_MAC_ADDR_STR_LEN] = {0};

    if (!estimatorState.debugModeEnabled) {
        cmdf(context, "estimator 'stats' command is only valid when "
                      "debug mode is enabled\n");
        return;
    }

    if (!cmd) {
        cmdf(context, "estimator 'stats' command must include MAC address\n");
        return;
    }

    const char *arg = cmdWordFirst(cmd);

    cmdGetCurrArgNullTerm(arg, &mac[0], CMD_MAC_ADDR_STR_LEN);
    const struct ether_addr *staAddr = ether_aton(mac);
    if (!staAddr) {
        cmdf(context, "estimator 'stats' command invalid MAC address: %s\n",
             arg);
        return;
    }

    stadbEntry_handle_t entry = stadb_find(staAddr);
    if (!entry) {
        cmdf(context, "estimator 'stats' unknown MAC address: "
                      lbMACAddFmt(":") "\n",
             lbMACAddData(staAddr->ether_addr_octet));
        return;
    }

    // Now determine the actual stats values to assign.
    wlanif_staStats_t stats;
    arg = cmdWordNext(arg);
    if (!cmdWordDigits(arg)) {
        cmdf(context, "estimator 'stats' requires numeric value for "
                      "RSSI value\n");
        return;
    }
    stats.rssi = (lbd_rssi_t) atoi(arg);

    arg = cmdWordNext(arg);
    if (!cmdWordDigits(arg)) {
        cmdf(context, "estimator 'stats' requires numeric value for "
                      "TX rate value\n");
        return;
    }
    stats.txRate = (lbd_linkCapacity_t) atoi(arg);

    arg = cmdWordNext(arg);
    if (!cmdWordDigits(arg)) {
        cmdf(context, "estimator 'stats' requires numeric value for "
                      "TX byte count value\n");
        return;
    }
    char *eptr;
    stats.txByteCount = strtoull(arg, &eptr, 10);

    arg = cmdWordNext(arg);
    if (!cmdWordDigits(arg)) {
        cmdf(context, "estimator 'stats' requires numeric value for "
                      "TX packet count value\n");
        return;
    }
    stats.txPacketCount = (u_int32_t) atoi(arg);

    // Verify that the STA is actually associated on this AP.
    stadbEntry_bssStatsHandle_t servingBSS;
    const lbd_bssInfo_t *servingBSSInfo;
    if (!estimatorCmnIsSelfServing(entry, &servingBSS, &servingBSSInfo)) {
        // Invalid entry or not associated. Cannot measure the stats.
        cmdf(context, lbMACAddFmt(":") " is not associated to this AP; "
             "cannot use estimator 'stats' command\n",
             lbMACAddData(staAddr->ether_addr_octet));
        return;
    }

    estimatorCmnHandleSTAStats(entry, staAddr, servingBSS, servingBSSInfo, &stats);
}

static const char *estimatorMenuDiaglogHelp[] = {
    "diaglog -- generate diaglog message",
    "Usage:",
    "\tdiaglog pollution: generate pollution status log for all polluted BSSes",
    NULL
};

/**
 * @brief Callback function to generate pollution diaglog if the given BSS is polluted
 *        for the given STA
 *
 * @param [in] entryHandle  the STA entry to generate diaglog
 * @param [in] bssHandle  the BSS to check if marked as polluted
 * @param [in] cookie  not used
 *
 * @return 0 as no BSS will be selected
 */
static u_int32_t estimatorCmnDiaglogPollutionBssCB(stadbEntry_handle_t entry,
                                                   stadbEntry_bssStatsHandle_t bssStats,
                                                   void *cookie) {
    LBD_BOOL polluted = LBD_FALSE;
    if (LBD_OK == stadbEntry_getPolluted(entry, bssStats, &polluted, NULL) &&
        polluted) {
        const lbd_bssInfo_t *bssInfo = stadbEntry_resolveBSSInfo(bssStats);
        const struct ether_addr *staAddr = stadbEntry_getAddr(entry);
        lbDbgAssertExit(estimatorState.dbgModule, bssInfo && staAddr);
        // Since it's not really a pollution change, an invalid change reason is used here
        estimatorCmnDiaglogSTAPollutionChanged(staAddr, bssInfo, LBD_TRUE /* polluted */,
                                               estimatorPollutionChangedReason_invalid);
    }

    return 0;
}

/**
 * @brief Callback function to generate pollution diaglog for all polluted BSSes
 *        for the given STA
 *
 * @param [in] entry  the entry to generate diaglog
 * @param [in] cookie  not used
 */
static void estimatorCmnDiaglogPollutionCallback(stadbEntry_handle_t entry, void *cookie) {
    // Do nothing on BSS iteration failure, so intentionally ignore the return value here.
    stadbEntry_iterateBSSStats(entry, estimatorCmnDiaglogPollutionBssCB, NULL, NULL, NULL);
}

#ifndef GMOCK_UNIT_TESTS
static
#endif
void estimatorMenuDiaglogHandler(struct cmdContext *context, const char *cmd) {
    const char *arg = cmdWordFirst(cmd);
#define DIAGLOG_POLLUTION "pollution"
    if (!arg) {
        cmdf(context, "estimator 'diaglog' command requires one argument\n");
        return;
    }
    if (strncmp(DIAGLOG_POLLUTION, arg, strlen(DIAGLOG_POLLUTION)) == 0) {
        if (stadb_iterate(estimatorCmnDiaglogPollutionCallback, NULL) == LBD_NOK) {
            cmdf(context, "'diaglog %s': Failed to iterate stadb\n", arg);
        }
    } else {
        cmdf(context, "estimator 'diaglog' unknown command: %s\n", arg);
    }
#undef DIAGLOG_POLLUTION
}

// Sub-menus for the estimator debug CLI.
static const struct cmdMenuItem estimatorMenu[] = {
    CMD_MENU_STANDARD_STUFF(),
    { "s", estimatorMenuStatusHandler, NULL, estimatorMenuStatusHelp },
    { "rate", estimatorMenuRateHandler, NULL, estimatorMenuRateHelp },
    { "airtime", estimatorMenuAirtimeHandler, NULL, estimatorMenuAirtimeHelp },
    { "d", estimatorMenuDebugHandler, NULL, estimatorMenuDebugHelp },
    { "detect", estimatorMenuInterferenceDetectedHandler, NULL,
      estimatorMenuInterferenceDetectedHelp },
    { "stats", estimatorMenuInterferenceStatsHandler, NULL,
      estimatorMenuInterferenceStatsHelp },
    { "diaglog", estimatorMenuDiaglogHandler, NULL, estimatorMenuDiaglogHelp },
    CMD_MENU_END()
};

// Top-level estimator help items
static const char *estimatorMenuHelp[] = {
    "estimator -- Rate estimator",
    NULL
};

// Top-level station monitor menu.
static const struct cmdMenuItem estimatorMenuItem = {
    "estimator",
    cmdMenu,
    (struct cmdMenuItem *) estimatorMenu,
    estimatorMenuHelp
};

#endif /* LBD_DBG_MENU */

/**
 * @brief Initialize the debug CLI hooks for this module (if necesary).
 */
static void estimatorMenuInit(void) {
#ifdef LBD_DBG_MENU
    cmdMainMenuAdd(&estimatorMenuItem);
#endif /* LBD_DBG_MENU */
}
