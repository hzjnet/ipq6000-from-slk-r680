// vim: set et sw=4 sts=4 cindent:
/*
 * @File: wlanifBSteerControlCmn.h
 *
 * @Abstract: Header for load balancing daemon band steering control interface
 *
 * @Notes:
 *
 * @@-COPYRIGHT-START-@@
 *
 * Copyright (c) 2015-2018 Qualcomm Technologies, Inc.
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
 */

#ifndef wlanifBSteerControlCmn__h
#define wlanifBSteerControlCmn__h

#include <stdlib.h>

#include <dbg.h>
#include <evloop.h>
#include <sys/types.h>  // for fixed-width integer types

#include "list.h"
#include "wlanifBSteerControl.h"

#include "ieee80211_external.h"

#if defined(__cplusplus)
extern "C" {
#endif

// ====================================================================
// Internal constants and types shared by BSA and MBSA
// ====================================================================

// Maximum number of VAPs on a single band
#define MAX_VAP_PER_BAND 16

// Enable RRM/WNM Host Filter from LBD
#define SET_RRM_FILTER 1
#define SET_WNM_FILTER 1

/**
 * @brief Structure used to define an ESS
 */
typedef struct wlanifBSteerControlEssInfo_t {
    // SSID length
    u_int8_t ssidLen;

    // SSID string
    u_int8_t ssidStr[IEEE80211_NWID_LEN+1];
} wlanifBSteerControlEssInfo_t;

/**
 * @brief Internal structure for the radios in the system.
 *
 * VAPs are enabled on a specific radio. This type is used to represent
 * characteristics and state of the radio that are shared across all
 * VAPs on the radio.
 */
struct wlanifBSteerControlRadioInfo {
    /// Flag indicating whether the entry is valid.
    LBD_BOOL valid : 1;

    /// Flag indicating if the radio has the highest Tx power on its band.
    /// For single radio, it is always LBD_TRUE
    LBD_BOOL strongestRadio : 1;

    /// The MAC address of the radio
    struct ether_addr radioAddr;

    /// Interface name, +1 to ensure it is null-terminated.
    char ifname[IFNAMSIZ + 1];

    /// The value for the private ioctl that sets whether the SON mode
    /// is enabled or not.
    int sonIoctl;

    /// The resolved number for the enable_ol_stats ioctl.
    int enableOLStatsIoctl;

    /// The resolved number for the Nodebug for direct attach hardware.
    int enableNoDebug;

    /// The number of calls to enable the stats that need to be disabled.
    size_t numEnableStats;

    /// Channel on which this radio is operating.
    lbd_channelId_t channel;

    /// Regulatory class in which this radio is operating.
    u_int8_t regClass;

    /// Maximum Tx power on this radio
    u_int8_t maxTxPower;

    /// Maximum channel width supported on this radio
    wlanif_chwidth_e maxChWidth;

    // a list of STAs whose RSSI measurement is requested
    list_head_t rssiWaitingList;

    /// Freq on which this radio is operating
    u_int16_t freq;
};

/**
 * @brief internal structure for VAP information
 */
struct wlanifBSteerControlVapInfo {
    // flag indicating if this VAP is valid
    LBD_BOOL valid;

    // interface name, +1 to ensure it is null-terminated
    char ifname[IFNAMSIZ + 1];

    /// Reference to the radio that "owns" this VAP.
    struct wlanifBSteerControlRadioInfo *radio;

    // system index
    int sysIndex;

    // Whether the interface is considered up or not
    LBD_BOOL ifaceUp;

    // MAC address of this VAP
    struct ether_addr macaddr;

    // PHY capabilities information
    wlanif_phyCapInfo_t phyCapInfo;

    // ID corresponding to the ESS
    lbd_essId_t essId;

    // Flag to indicate whether the VAP is included for Bandsteering or excluded for Bandsteering
    LBD_BOOL includedIface;
};

/**
 * @brief internal structure for band information
 */
struct wlanifBSteerControlBandInfo {
    // All VAPs on this band
    struct wlanifBSteerControlVapInfo vaps[MAX_VAP_PER_BAND];

    // config parameters
    ieee80211_bsteering_param_t configParams;

    // flag indicating if band steering is enabled on this band
    LBD_BOOL enabled;

    // duration for 802.11k beacon report
    u_int32_t bcnrptDurations[IEEE80211_RRM_BCNRPT_MEASMODE_RESERVED][BSTEERING_MAX_CLIENT_CLASS_GROUP];

    // flag indicating if ackrssi is enabled for this band
    u_int8_t ackrssi;
};

struct wlanifBSteerControlPriv_t {
    struct dbgModule *dbgModule;

    struct wlanifBSteerControlRadioInfo radioInfo[WLANIF_MAX_RADIOS];

    struct wlanifBSteerControlBandInfo bandInfo[wlanif_band_invalid];

    // Socket used to send control request down to driver
    int controlSock;

    /// Timer used to periodically check whether ACS and CAC have completed
    struct evloopTimeout vapReadyTimeout;

    /// Flag indicating whether any VAPs are being managed.
    LBD_BOOL hasZeroAPIfaces;

    /// Flag indicating whether band steering is currently enabled.
    LBD_BOOL bandSteeringEnabled;

    /// Number of ESSes supported on this device
    u_int8_t essCount;

    /// Variable used to set or clear the Authentication allow feature
    u_int8_t auth[BSTEERING_MAX_CLIENT_CLASS_GROUP];

    // Flag to indicate if 'Blacklist install on Other ESS' is enabled
    u_int8_t blacklistOtherESS;

    // Flag indicating if tx should be used for inactivity detection
    LBD_BOOL inactDetectionFromTx;

    // variable to hold semaphore id
    int semid;

    /// Structure used to map ESS string to an ID (for
    /// simpler comparisons.  Each VAP on a radio must have a
    /// unique ESSID.  Index into this array will be the
    /// essId.
    wlanifBSteerControlEssInfo_t essInfo[MAX_VAP_PER_BAND];

// For now, we are only permitting two observers, as it is likely that the
// following will need to observe channel change
//
// 1. Station database
// 2. Steering executor
#define MAX_CHAN_CHANGE_OBSERVERS 2
    /// Observer for channel change
    struct wlanifBSteerControlChanChangeObserver {
        LBD_BOOL isValid;
        wlanif_chanChangeObserverCB callback;
        void *cookie;
    } chanChangeObserver[MAX_CHAN_CHANGE_OBSERVERS];

    // Flag to indicate multicast enabled or not on NETLINK sockets
    u_int8_t nlSteerMulticast;

    /* Ack RSSI Enable */
    u_int8_t ackRssiEnable;

    /* Log Mod Counter */
    u_int32_t logModCounter;
};

typedef struct ath_vendorcfg_event {
    /* Type of VENDOR_CFG_EVENT.*/
    int type;
    /* The OS-specific index of the VAP on which the event occurred.*/
    int sys_index;
    /* The data for the event. Which member is valid is based on the
       type field.*/
    union {
        struct ieee80211_hmwds_ast_add_status ast_status;
        struct ev_rrm_report_data rrm_report;
        struct ieee80211_bstmquery_ev_data wnm_query;
        struct ieee80211_bstmresp_ev_data wnm_response;
        struct ieee80211_smps_update_data smps_update;
        struct ieee80211_opmode_update_data opmode_update;
    } data;
} ath_vendorcfg_event_t;

/**
 * @brief React to an indication from the driver that SM Power Save mode
 *        updated for a given STA
 *
 * @param [in] state, pointer to control handle
 * @param [in] event  data from vendor cfg
 * @param [in] bss BSS the event was received from
 */
void wlanifVendorCFGEventsHandleSMPSUpdateInd(
    wlanifBSteerControlHandle_t  state,
    ath_vendorcfg_event_t *event,
    lbd_bssInfo_t *bss);

/**
 * @brief React to an indication from the driver that Operating Mode Notification
 *        received from a given STA
 *
 * @param [in] state, pointer to control handle
 * @param [in] event  data from vendor cfg
 * @param [in] bss BSS the event was received from
 */
void wlanifVendorCFGEventsHandleOpModeUpdateInd(
    wlanifBSteerControlHandle_t  state,
    ath_vendorcfg_event_t *event,
    lbd_bssInfo_t *bss);

/**
 * @brief React to an indication from the driver that it
 *        received a WNM event.
 *
 * @param [in] state, pointer to control handle
 * @param [in] event  data from vendor cfg
 * @param [in] bss BSS the event was received from
 */
void wlanifVendorCFGEventsHandleWNMEvent(
    wlanifBSteerControlHandle_t  state,
    ath_vendorcfg_event_t *event,
    lbd_bssInfo_t *bss);

/**
 * @brief React to an indication from the driver that an 802.11k radio
 *        resource measurement report has been received.
 *
 * @param [in] state, pointer to control handle
 * @param [in] event  data from vendor cfg
 * @param [in] bss BSS the event was received from
 */
void wlanifVendorCFGEventsHandleRRMReportInd(
    wlanifBSteerControlHandle_t  state,
    ath_vendorcfg_event_t *event,
    lbd_bssInfo_t *bss);

// ====================================================================
// Functions shared internally by BSA and MBSA
// ====================================================================

/**
 * @brief Fill in a local lbd_bssInfo_t for the VAP that matches
 *        the BSSID
 *
 * @param [in] state  the handle returned from wlanifBSteerControlCreate()
 * @param [in] bssid  BSSID for this VAP
 * @param [out] bss  structure to be filled in with BSS info
 *
 * @return LBD_STATUS LBD_OK if the BSS was found, LBD_NOK
 *                    otherwise
 */
LBD_STATUS wlanifBSteerControlCmnGetLocalBSSInfoFromBSSID(
    wlanifBSteerControlHandle_t state, const u_int8_t *bssid,
    lbd_bssInfo_t *bss);

/**
 * @brief Store the SSID that a VAP is operating on.  If this
 *        SSID is already known, just the essId index will be
 *        stored.  If the SSID is not known, a new essId index
 *        will be assigned.
 *
 * @param [in] state the "this" pointer
 * @param [in] ifname name of the interface this VAP is
 *                    operating on
 * @param [in] vap pointer to VAP structure
 * @param [in] length length of the SSID string for this VAP
 * @param [in] ssidStr SSID string for this VAP
 *
 * @return LBD_STATUS returns LBD_OK if the SSID is valid,
 *         LBD_NOK otherwise
 */
LBD_STATUS wlanifBSteerControlCmnStoreSSID(
    wlanifBSteerControlHandle_t state, const char *ifname,
    struct wlanifBSteerControlVapInfo *vap, u_int8_t length,
    u_int8_t *ssidStr);

/**
 * @brief Resolve the BSSID for a given set of BSS info parameters.
 *
 * @param [in] bssInfo  the parameters to look up; this must be for the self
 *                      AP ID
 *
 * @return the BSSID, or NULL if it cannot be resolved
 */
const struct ether_addr *wlanifBSteerControlCmnGetBSSIDForLocalBSSInfo(
        const lbd_bssInfo_t *bssInfo);

/**
 * @brief Find BSSes that are on the same ESS with the given BSS (except the
 *        given one), or all BSSes on the given ESS
 *
 * This only works when the serving BSS is a local one.
 *
 * @param [in] handle  the handle returned from wlanifBSteerControlCreate()
 * @param [in] bss  the given BSS
 * @param [in] lastServingESS  if no given BSS, will find BSSes on this ESS
 * @param [in] band  if set to wlanif_band_invalid, find BSSes from both bands;
 *                   otherwise, find BSSes from the given band
 * @param [inout] maxNumBSSes  on input, it is the maximum number of BSSes expected;
 *                             on output, return number of entries in the bssList
 * @param [out] bssList  the list of BSSes that are on the same ESS with given BSS
 *
 * @return LBD_OK on success, otherwise return LBD_NOK
 */
LBD_STATUS wlanifBSteerControlGetBSSesSameESSLocal(
        wlanifBSteerControlHandle_t state, const lbd_bssInfo_t *bss,
        lbd_essId_t lastServingESS, wlanif_band_e requestedBand,
        size_t* maxNumBSSes, lbd_bssInfo_t *bssList, u_int8_t supported_band);

/**
 * @brief Find the interface status of the given BSS
 *
 * @param [in] bss  the given BSS
 *
 * @return LBD_OK if interface is up, otherwise return LBD_NOK
 */
LBD_STATUS  wlanifBSteerControlGetIfaceStatus(const lbd_bssInfo_t *bss);

/**
 * @brief Obtain a copy of the PHY capabilities of a given local BSS.
 *
 * @param [in] handle  the handle returned from wlanifBSteerControlCreate()
 *                     to use for obtaining the capabilties
 * @param [in] bss  the BSS for which to obtain the capabilities
 * @param [out] phyCap  on success, the PHY capabilities
 *
 * @return LBD_OK on success; otherwise LBD_NOK
 */
LBD_STATUS wlanifBSteerControlCmnGetLocalBSSPHYCapInfo(
        wlanifBSteerControlHandle_t state, const lbd_bssInfo_t *bss,
        wlanif_phyCapInfo_t *phyCap);

/**
 * @brief Resolve the SSID for an ESS
 *
 * @param [in] state  the handle returned from wlanifBSteerControlCreate()
 *                    to use for obtaining the SSID
 * @param [in] essId  the ID of the ESS for which to resolve the SSID
 * @param [out] ssid  the SSID for the given ESS
 * @param [inout] len  on input it is the max length of the output SSID
 *                     on output it is the length of the SSID for the given ESS
 *
 * @return LBD_OK on success; otherwise LBD_NOK
 */
LBD_STATUS wlanifBSteerControlCmnResolveSSID(wlanifBSteerControlHandle_t state,
                                             lbd_essId_t essId, char *ssid,
                                             u_int8_t *len);

// ====================================================================
// Protected functions
// ====================================================================

/**
 * @brief Trigger the generation of events for changes in the radio operating
 *        channel.
 *
 * This is generally only invoked at startup time.
 *
 * @param [in] state  the handle returned from wlanifBSteerControlCreate()
 *
 * @return LBD_OK on successful operation; otherwise LBD_NOK
 */
LBD_STATUS wlanifBSteerControlGenerateRadioOperChanChangeEvents(
    wlanifBSteerControlHandle_t state);

/**
 * @brief Notify other modules after one or more radio's operating channel
 *        information has been updated.
 *
 * @param [in] state  the handle returned from wlanifBSteerControlCreate()
 */
void wlanifBSteerControlNotifyRadioOperChanChange(wlanifBSteerControlHandle_t state);

/**
 * @brief Determine whether the driver should be told that SON mode is
 *        enabled.
 *
 * SON mode is the mode reserved for the fully coordinated multi-AP steering.
 *
 * @param [in] state  the handle returned from wlanifBSteerControlCreate()
 *
 * @return LBD_TRUE if SON mode should be set in the driver;
 *         otherwise LBD_FALSE
 */
LBD_BOOL wlanifBSteerControlGetSONInitVal(wlanifBSteerControlHandle_t state);

/**
 * @brief Send request down to driver using ioctl() for SET operations.
 *
 * @see wlanifBSteerControlCmnSendVAP for parameters and return value explanation
 */
LBD_STATUS wlanifBSteerControlCmnSetSendVAP(wlanifBSteerControlHandle_t state, const char *ifname,
                                            u_int8_t cmd, const struct ether_addr *destAddr,
                                            void *data, int data_len, LBD_BOOL ignoreError);

/**
 * @brief Get the VAP with a matching channel
 *
 * @param [in] state the 'this' pointer
 * @param [in] channelId  the channel to search for
 *
 * @pre state must be valid
 * @pre channelId must be valid
 *
 * @return pointer to the first VAP with a matching channel; otherwise NULL
 */
struct wlanifBSteerControlVapInfo *wlanifBSteerControlCmnGetFirstVAPByRadio(
    wlanifBSteerControlHandle_t state, const struct wlanifBSteerControlRadioInfo *radio);

/**
 * @brief Function to get whether multicast
 *        option is enabled on NL socket
 * @param [in] state the 'this' pointer
 * @param [out]  multicast  enable option
 *
 */
LBD_STATUS wlanifBSteerControlGetLocalNLSteerMulticast(
        wlanifBSteerControlHandle_t state,
        u_int8_t *nlSteerMulticast);

LBD_STATUS wlanifBSteerControlCmnSendAckRSSIEnable(
        wlanifBSteerControlHandle_t state,
                                 wlanif_band_e band,
                                 LBD_BOOL ignoreError);

LBD_STATUS wlanifBSteerControlCmnSetAckRSSIEachBand(
                               wlanifBSteerControlHandle_t state,
                               wlanif_band_e band);

LBD_STATUS wlanifBSteerControlCmnSetAckRSSI(
                                 wlanifBSteerControlHandle_t state);

LBD_STATUS wlanifBSteerControlGetLocalAckRssiEnable(
        wlanifBSteerControlHandle_t state,
        u_int8_t *ack_rssi_enable);

LBD_STATUS wlanifBSteerControlGetLocalModCounter(
        wlanifBSteerControlHandle_t state,
        u_int32_t *modCounter);

#if defined(__cplusplus)
}
#endif

#endif  // wlanifBSteerControlCmn__h
