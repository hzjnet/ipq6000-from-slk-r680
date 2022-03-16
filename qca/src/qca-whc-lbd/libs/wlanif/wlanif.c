// vim: set et sw=4 sts=4 cindent:
/*
 * @File: wlanif.c
 *
 * @Abstract: Load balancing daemon WLAN interface
 *
 * @Notes:
 *
 * @@-COPYRIGHT-START-@@
 *
 * Copyright (c) 2014-2018 Qualcomm Technologies, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 * 2014-2016 Qualcomm Atheros, Inc.
 *
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
 * @@-COPYRIGHT-END-@@
 *
 */


#include "wlanif.h"
#include "wlanifBSteerEvents.h"
#include "wlanifLinkEvents.h"
#include "wlanifBSteerControl.h"
#include "wlanifBSteerControlCmn.h"
#include "wlanifPrivate.h"

#include <stdlib.h>
#include <string.h>

#include <dbg.h>
#include <cmd.h>
#include <limits.h>

#include "module.h"
#include "lb_common.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/file.h>

extern void isCfg80211_lbd(int);

static struct {
    wlanifBSteerControlHandle_t bsteerControlHandle;
    wlanifBSteerEventsHandle_t bsteerEventsHandle;
    wlanifLinkEventsHandle_t linkEventsHandle;

    struct dbgModule *dbgModule;
} wlanifState;

static void wlanifListenInitCB(void) {
    if ( wlanifBSteerControlEventsEnable(wlanifState.bsteerControlHandle,
                                        wlanifState.bsteerEventsHandle) != LBD_OK) {
        // An error will already have been printed. Just exist with a
        // failure code.
        exit(1);
    }
}

// ====================================================================
// Package level API
// ====================================================================

wlanif_band_e wlanifMapFreqToBand(int32_t freq) {
    if (freq <= 1000) {
        // It is a channel number. The values here for 2.4 GHz versus
        // 5 GHz are a start. It's not clear if these are right in
        // every regulator domain.
        if (freq < 27) {
            return wlanif_band_24g;
        } else {
            return wlanif_band_5g;
        }
    } else {
        // Value is a raw frequency (seems to be in 10s of Hz).
        if (freq / 100000 >= WLANIF_6G_START_FREQ)
            return wlanif_band_6g;
        else if (freq / 100000000 >= 5) {
            return wlanif_band_5g;
        } else {
            return wlanif_band_24g;
        }
   }
}

// ====================================================================
// Public API
// ====================================================================

void wlanif_setCfg80211 (int isCfg80211)
{
    isCfg80211_lbd(isCfg80211);
}

LBD_STATUS wlanif_init_internal(void) {
    wlanifState.dbgModule = dbgModuleFind("wlanif");
    wlanifState.dbgModule->Level = DBGINFO;

    wlanifState.bsteerControlHandle =
        wlanifBSteerControlCreate(wlanifState.dbgModule);

    if (!wlanifState.bsteerControlHandle) {
        return LBD_NOK;
    }

    mdEventTableRegister(mdModuleID_WlanIF, wlanif_event_maxnum);
    mdListenInitCBRegister(mdModuleID_WlanIF, wlanifListenInitCB);

    // bsteerEventsHandle must be created after bsteerControlHandle
    wlanifState.bsteerEventsHandle =
        wlanifBSteerEventsCreate(wlanifState.dbgModule,
                                 wlanifState.bsteerControlHandle);

    if (!wlanifState.bsteerEventsHandle) {
        return LBD_NOK;
    }

    // linkEventsHandle must be created after bsteerControlHandle
    wlanifState.linkEventsHandle =
        wlanifLinkEventsCreate(wlanifState.dbgModule,
                               wlanifState.bsteerControlHandle);

    if (!wlanifState.linkEventsHandle) {
        return LBD_NOK;
    }


    LBD_BOOL enabled = LBD_FALSE;
    if (wlanifBSteerControlEnableWhenReady(wlanifState.bsteerControlHandle,
                                           &enabled) == LBD_NOK) {
        return LBD_NOK;
    }
    if (wlanifBSteerControlCmnSetAckRSSI(wlanifState.bsteerControlHandle)
                                                             == LBD_NOK) {
        return LBD_NOK;
    }

    return LBD_OK;
}

LBD_STATUS wlanif_init(void) {
    /* lock the call to avoid race condition between hyd daemons */
    int fd_lock = open(HYD_MY_LOCK, O_CREAT);
    if(fd_lock >= 0) {
        flock(fd_lock, LOCK_EX);
    }

    LBD_STATUS ret = wlanif_init_internal();
    if(fd_lock >= 0) {
        flock(fd_lock, LOCK_UN);
        close(fd_lock);
    }

    return ret;
}

LBD_STATUS wlanif_generateRadioOperChanChangeEvents(void) {
    if (!wlanifState.bsteerControlHandle) {
        return LBD_NOK;
    }

    return wlanifBSteerControlGenerateRadioOperChanChangeEvents(
            wlanifState.bsteerControlHandle);
}

LBD_STATUS wlanif_setAPSteeringRSSIThreshold(
        const struct ether_addr *radioAddr, lbd_rssi_t rssi,
        wlanif_band_e *band) {
    if (!wlanifState.bsteerControlHandle) {
        return LBD_NOK;
    }

    return wlanifBSteerControlSetAPSteeringRSSIThreshold(
            wlanifState.bsteerControlHandle, radioAddr, rssi, band);
}

LBD_STATUS wlanif_setRSSIMetricsReportingParams(
        const struct ether_addr *radioAddr, lbd_rssi_t rssi,
        u_int8_t hysteresis, wlanif_band_e *band) {
    if (!wlanifState.bsteerControlHandle) {
        return LBD_NOK;
    }

    return wlanifBSteerControlSetRSSIMetricsReportingParams(
            wlanifState.bsteerControlHandle, radioAddr, rssi, hysteresis,
            band);
}

LBD_STATUS wlanif_sendBTMRequestMap(const lbd_bssInfo_t *assocBSS,
                                 const struct ether_addr *staAddr,
                                 u_int8_t dialogToken,
                                 u_int16_t disassocTimer,
                                 LBD_BOOL disassocImminent,
                                 LBD_BOOL abridgedBit,
                                 const wlanif_bssTargetInfo_t *candidate) {
    if (!wlanifState.bsteerControlHandle) {
        return LBD_NOK;
    }

    return wlanifBSteerControlSendBTMRequestMapParams(wlanifState.bsteerControlHandle, assocBSS,
                                                      staAddr, dialogToken, disassocTimer,
                                                      disassocImminent, abridgedBit, candidate);
}

LBD_STATUS wlanif_setOverload(lbd_channelId_t channel, u_int16_t freq, LBD_BOOL overload) {
    if (!wlanifState.bsteerControlHandle) {
        return LBD_NOK;
    }

    return wlanifBSteerControlSetOverload(wlanifState.bsteerControlHandle,
                                          channel, freq, overload);
}

LBD_STATUS wlanif_dumpAssociatedSTAs(wlanif_associatedSTAsCB callback,
                                     void *cookie) {
    if (!wlanifState.bsteerControlHandle) {
        return LBD_NOK;
    }

    return wlanifBSteerControlDumpAssociatedSTAs(wlanifState.bsteerControlHandle,
                                                 callback, cookie);
}

LBD_STATUS wlanif_requestStaRSSI(const lbd_bssInfo_t *bss,
                                 const struct ether_addr * staAddr,
                                 u_int8_t numSamples) {
    if (!wlanifState.bsteerControlHandle) {
        return LBD_NOK;
    }

    return wlanifBSteerControlRequestStaRSSI(wlanifState.bsteerControlHandle,
                                             bss, staAddr, numSamples);
}

LBD_STATUS wlanif_setChannelStateForSTA(
    u_int8_t channelCount,
    const lbd_channelId_t *channelList,
    const u_int16_t *freqList,
    const struct ether_addr *staAddr,
    lbd_essId_t lastServingESS,
    LBD_BOOL enable) {
    if (!wlanifState.bsteerControlHandle) {
        return LBD_NOK;
    }

    return wlanifBSteerControlSetChannelStateForSTA(
            wlanifState.bsteerControlHandle, channelCount, channelList, freqList,
            lastServingESS, staAddr, enable);
}

LBD_STATUS wlanif_setChannelProbeStateForSTA(
    wlanif_band_e supported_band,
    u_int8_t channelCount,
    const lbd_channelId_t *channelList,
    const u_int16_t *freqList,
    const struct ether_addr *staAddr,
    LBD_BOOL enable) {
    if (!wlanifState.bsteerControlHandle) {
        return LBD_NOK;
    }

    return wlanifBSteerControlSetChannelProbeStateForSTA(supported_band,
            wlanifState.bsteerControlHandle, channelCount, channelList,
            freqList, staAddr, enable);
}

LBD_STATUS wlanif_setNonCandidateStateForSTA(
    u_int8_t candidateCount,
    const lbd_bssInfo_t *candidateList,
    const struct ether_addr *staAddr,
    lbd_essId_t lastServingESS,
    LBD_BOOL enable,
    LBD_BOOL probeOnly,
    const lbd_bssInfo_t *servingBSSInfo,
    u_int8_t clientClassGroup) {
    if (!wlanifState.bsteerControlHandle) {
        return LBD_NOK;
    }

    return wlanifBSteerControlSetNonCandidateStateForSTA(
            wlanifState.bsteerControlHandle, candidateCount, candidateList,
            staAddr, lastServingESS, enable, probeOnly, servingBSSInfo,
            clientClassGroup);
}

u_int8_t wlanif_getNonCandidateStateForSTA(
    u_int8_t candidateCount,
    const lbd_bssInfo_t *candidateList,
    u_int8_t maxCandidateCount,
    lbd_bssInfo_t *outCandidateList,
    const lbd_bssInfo_t *servingBSSInfo) {
    if (!wlanifState.bsteerControlHandle) {
        return 0;
    }

    return wlanifBSteerControlGetNonCandidateStateForSTA(
            wlanifState.bsteerControlHandle, candidateCount, candidateList,
            maxCandidateCount, outCandidateList, servingBSSInfo);
}

LBD_STATUS wlanif_setCandidateStateForSTA(
    u_int8_t candidateCount,
    const lbd_bssInfo_t *candidateList,
    const struct ether_addr *staAddr,
    lbd_essId_t lastServingESS,
    LBD_BOOL enable,
    const lbd_bssInfo_t *servingBSSInfo,
    u_int16_t validitySecs,
    u_int8_t clientClassGroup) {
    if (!wlanifState.bsteerControlHandle) {
        return LBD_NOK;
    }

    if (wlanifBSteerControlPerformIoctlExcludedVaps(
            wlanifState.bsteerControlHandle, staAddr, enable) == LBD_NOK) {
        dbgf(wlanifState.dbgModule, DBGERR,
             "%s: Error in blacklisting on the Excluded VAPs", __func__);
        return LBD_NOK;
    }

    if(wlanifState.bsteerControlHandle->blacklistOtherESS){
        if (wlanifBSteerControlPerformIoctlOtherEss(
                    wlanifState.bsteerControlHandle, lastServingESS, staAddr, enable) == LBD_NOK) {
            dbgf(wlanifState.dbgModule, DBGERR,
                    "%s: Error in blacklisting on the other ESS", __func__);
            return LBD_NOK;
        }
    }

    return wlanifBSteerControlSetCandidateStateForSTA(
            wlanifState.bsteerControlHandle, candidateCount, candidateList,
            staAddr, enable, servingBSSInfo, validitySecs, clientClassGroup);
}

LBD_STATUS wlanif_setCandidateProbeStateForSTA(
    u_int8_t candidateCount,
    const lbd_bssInfo_t *candidateList,
    const struct ether_addr *staAddr,
    LBD_BOOL enable) {
    if (!wlanifState.bsteerControlHandle) {
        return LBD_NOK;
    }

    return wlanifBSteerControlSetCandidateProbeStateForSTA(
            wlanifState.bsteerControlHandle, candidateCount, candidateList,
            staAddr, enable);
}

LBD_BOOL wlanif_isBSSIDInList(u_int8_t candidateCount,
                              const lbd_bssInfo_t *candidateList,
                              const struct ether_addr *bssid) {
    if (!wlanifState.bsteerControlHandle) {
        return LBD_FALSE;
    }

    return wlanifBSteerControlIsBSSIDInList(
            wlanifState.bsteerControlHandle, candidateCount, candidateList,
            bssid);
}

const struct ether_addr *wlanif_getBSSIDForBSSInfo(
        const lbd_bssInfo_t *bssInfo) {
    if (!wlanifState.bsteerControlHandle) {
        return NULL;
    }

    return wlanifBSteerControlGetBSSIDForBSSInfo(wlanifState.bsteerControlHandle,
                                                 bssInfo);
}

LBD_STATUS wlanif_getBSSInfoFromBSSID(const struct ether_addr *bssid,
                                      lbd_bssInfo_t *bssInfo) {
    if (!bssid || !wlanifState.bsteerControlHandle) {
        return LBD_NOK;
    }

    // In the absence of real ESSID information, we just provide the default
    // one (of 0).
    return wlanifBSteerControlGetBSSInfoFromBSSID(
            wlanifState.bsteerControlHandle,
            bssid->ether_addr_octet, bssInfo);
}

u_int8_t wlanif_getChannelList(lbd_channelId_t *channelList,
                               wlanif_chwidth_e *chwidthList,
                               u_int16_t *freqList,
                               u_int8_t maxSize) {
    if (!wlanifState.bsteerControlHandle) {
        return 0;
    }

    return wlanifBSteerControlGetChannelList(wlanifState.bsteerControlHandle,
                                             channelList, chwidthList, freqList, maxSize);
}

LBD_STATUS wlanif_disassociateSTA(const lbd_bssInfo_t *assocBSS,
                                  const struct ether_addr *staAddr,
                                  LBD_BOOL local) {
    if (!wlanifState.bsteerControlHandle) {
        return LBD_NOK;
    }

    return wlanifBSteerControlDisassociateSTA(
            wlanifState.bsteerControlHandle, assocBSS, staAddr, local);
}

LBD_STATUS wlanif_sendBTMRequest(const lbd_bssInfo_t *assocBSS,
                                 const struct ether_addr *staAddr,
                                 u_int8_t dialogToken,
                                 u_int8_t candidateCount,
                                 u_int8_t steerReason,
                                 u_int8_t forceSteer,
                                 const lbd_bssInfo_t *candidateList) {

    if (!wlanifState.bsteerControlHandle) {
        return LBD_NOK;
    }

    return wlanifBSteerControlSendBTMRequest(
            wlanifState.bsteerControlHandle, assocBSS, staAddr,
            dialogToken, candidateCount, steerReason, forceSteer, candidateList);
}

LBD_STATUS wlanif_requestDownlinkRSSI(const lbd_bssInfo_t *bss,
                                      const struct ether_addr *staAddr,
                                      LBD_BOOL rrmCapable,
                                      size_t numChannels,
                                      const lbd_channelId_t *channelList,
                                      const uint16_t *freqList,
                                      u_int8_t clientClassGroup,
                                      LBD_BOOL useBeaconTable) {
    if (!wlanifState.bsteerControlHandle) {
        return LBD_NOK;
    }

    return wlanifBSteerControlRequestDownlinkRSSI(
               wlanifState.bsteerControlHandle, bss, staAddr,
               rrmCapable, numChannels, channelList, freqList,
               clientClassGroup, useBeaconTable);
}

LBD_STATUS wlanif_enableSTAStats(const lbd_bssInfo_t *bss) {
    // Callee will check that the handle is valid.
    return wlanifBSteerControlEnableSTAStats(
            wlanifState.bsteerControlHandle, bss);
}

LBD_STATUS wlanif_sampleSTAStats(const lbd_bssInfo_t *bss,
                                 const struct ether_addr *staAddr,
                                 LBD_BOOL rateOnly,
                                 wlanif_staStatsSnapshot_t *staStats) {
    // Callee will check that the handle is valid.
    return wlanifBSteerControlSampleSTAStats(
            wlanifState.bsteerControlHandle, bss, staAddr,
            rateOnly, staStats);
}

LBD_STATUS wlanif_disableSTAStats(const lbd_bssInfo_t *bss) {
    // Callee will check that the handle is valid.
    return wlanifBSteerControlDisableSTAStats(
            wlanifState.bsteerControlHandle, bss);
}

LBD_STATUS wlanif_getBSSPHYCapInfo(const lbd_bssInfo_t *bss,
                                   wlanif_phyCapInfo_t *phyCap) {
    // Callee will check that the handle is valid.
    return wlanifBSteerControlGetBSSPHYCapInfo(
            wlanifState.bsteerControlHandle, bss, phyCap);
}

LBD_STATUS wlanif_dumpATFTable(wlanif_reservedAirtimeCB callback,
                               void *cookie) {
    if (!wlanifState.bsteerControlHandle) {
        return LBD_NOK;
    }

    return wlanifBSteerControlDumpATFTable(wlanifState.bsteerControlHandle,
                                           callback, cookie);
}

LBD_BOOL wlanif_isSTAAssociated(const lbd_bssInfo_t *bss,
                                const struct ether_addr *staAddr) {
    if (!wlanifState.bsteerControlHandle) {
        return LBD_FALSE;
    }

    return wlanifBSteerControlIsSTAAssociated(wlanifState.bsteerControlHandle,
                                              bss, staAddr);
}

LBD_STATUS wlanif_registerChanChangeObserver(wlanif_chanChangeObserverCB callback,
                                             void *cookie) {
    if (!wlanifState.bsteerControlHandle) {
        return LBD_NOK;
    }

    return wlanifBSteerControlRegisterChanChangeObserver(
               wlanifState.bsteerControlHandle, callback, cookie);
}

LBD_STATUS wlanif_unregisterChanChangeObserver(wlanif_chanChangeObserverCB callback,
                                               void *cookie) {
    if (!wlanifState.bsteerControlHandle) {
        return LBD_NOK;
    }

    return wlanifBSteerControlUnregisterChanChangeObserver(
               wlanifState.bsteerControlHandle, callback, cookie);
}

LBD_STATUS wlanif_registerSTAStatsObserver(wlanif_staStatsObserverCB callback,
                                           void *cookie) {
    if (!wlanifState.bsteerEventsHandle) {
        return LBD_NOK;
    }

    return wlanifBSteerEventsRegisterSTAStatsObserver(
               wlanifState.bsteerEventsHandle, callback, cookie);
}

LBD_STATUS wlanif_unregisterSTAStatsObserver(wlanif_staStatsObserverCB callback,
                                             void *cookie) {
    if (!wlanifState.bsteerEventsHandle) {
        return LBD_NOK;
    }

    return wlanifBSteerEventsUnregisterSTAStatsObserver(
               wlanifState.bsteerEventsHandle, callback, cookie);
}

LBD_STATUS wlanif_isStrongestChannel(lbd_channelId_t channelId, LBD_BOOL *isStrongest) {
    return wlanifBSteerControlIsStrongestChannel(wlanifState.bsteerControlHandle,
                                                 channelId, isStrongest);
}

LBD_STATUS wlanif_isBSSOnStrongestChannel(const lbd_bssInfo_t *bss,
                                          LBD_BOOL *isStrongest) {
    return wlanifBSteerControlIsBSSOnStrongestChannel(
               wlanifState.bsteerControlHandle, bss, isStrongest);
}

LBD_STATUS wlanif_getBSSesSameESS(
        const lbd_bssInfo_t *bss, lbd_essId_t lastServingESS,
        wlanif_band_e band, size_t* maxNumBSSes, lbd_bssInfo_t *bssList,
        u_int8_t supported_band) {
    return wlanifBSteerControlGetBSSesSameESS(
               wlanifState.bsteerControlHandle, bss, lastServingESS,
               band, maxNumBSSes, bssList, supported_band);
}

LBD_STATUS wlanif_getIfaceStatus(const lbd_bssInfo_t *bss) {
    return wlanifBSteerControlGetIfaceStatus(bss);
}

LBD_STATUS wlanif_updateSteeringStatus(const struct ether_addr *addr,
                                       const lbd_bssInfo_t *bss,
                                       LBD_BOOL steeringInProgress) {
    return wlanifBSteerControlUpdateSteeringStatus(
               wlanifState.bsteerControlHandle, addr, bss,
               steeringInProgress);
}

LBD_STATUS wlanif_resolveRegClass(u_int8_t channel, u_int16_t freq, u_int8_t *regClass) {
    if (!regClass) {
        return LBD_NOK;
    }

    *regClass = IEEE80211_RRM_REGCLASS_RESERVED;

    if ( ( (freq >= WLANIF_2G_START_FREQ) && (freq <= WLANIF_2G_END_FREQ) ) ||
         ( (freq >= WLANIF_5G_START_FREQ) && (freq <= WLANIF_5G_END_FREQ) ) ) {
        if ((channel >= 1) && (channel <= 13)) {
            *regClass = IEEE80211_RRM_REGCLASS_81; /* 2.407 GHz, channels 1..13 */
            return LBD_OK;
        } else if (channel == 14) {
            *regClass = IEEE80211_RRM_REGCLASS_82; /* channel 14 */
            return LBD_OK;
        } else if ((channel >= 36) && (channel <= 48)) {
            *regClass = IEEE80211_RRM_REGCLASS_115; /* 5 GHz, channels 36..48 */
            return LBD_OK;
        } else if ((channel >= 149) && (channel <= 169)) {
            *regClass = IEEE80211_RRM_REGCLASS_125; /* 5 GHz, channels 149..169 */
            return LBD_OK;
        } else if (channel == 16) {
        // @todo: Is there a way to disambiguate the channels 8 and 12
        // from the 2.4GHz channels?
            *regClass = IEEE80211_RRM_REGCLASS_112; /* 5 GHz, channels 8, 12, 16 */
            return LBD_OK;
        } else if ((channel == 52) || (channel == 56) || (channel == 60) || (channel == 64)) {
            *regClass = IEEE80211_RRM_REGCLASS_118; /* 5 GHz, channels 52, 56, 60, 64 */
            return LBD_OK;
        } else if ((channel == 100) || (channel == 104) || (channel == 108) ||
               (channel == 112) || (channel == 116) || (channel == 120) ||
               (channel == 124) || (channel == 128) || (channel == 132) ||
               (channel == 136) || (channel == 140) || (channel == 144)) {
            *regClass = IEEE80211_RRM_REGCLASS_121; /* 5 GHz, channels 100, 104, 108, 112,
                           * 116, 120, 124, 128, 132, 136, 140, 144 */
            return LBD_OK;
        }
    } else if ( (freq >= WLANIF_6G_START_FREQ) && (freq <= WLANIF_6G_END_FREQ) ) {
        if (channel == 2)
            *regClass = IEEE80211_RRM_REGCLASS_136;
        else
            *regClass = IEEE80211_RRM_REGCLASS_131;

        return LBD_OK;
    }

    return LBD_NOK;
}

LBD_STATUS wlanif_resolveSSID(lbd_essId_t essId, char *ssid, u_int8_t *len) {
    return wlanifBSteerControlCmnResolveSSID(wlanifState.bsteerControlHandle, essId, ssid,
                                             len);
}

LBD_STATUS wlanif_fini() {
    /* lock the call to avoid race condition between hyd daemons */
    int fd_lock = open(HYD_MY_LOCK, O_CREAT);

    if(fd_lock >= 0) {
        flock(fd_lock, LOCK_EX);
    }

    LBD_STATUS status = LBD_OK;
    // Disable band steering feature first

    status |=
           wlanifControlClearRRMWNMFilter(wlanifState.bsteerControlHandle);

    status |=
        wlanifBSteerControlDisable(wlanifState.bsteerControlHandle, LBD_FALSE);

    status |=
        wlanifBSteerEventsDestroy(wlanifState.bsteerEventsHandle);
    wlanifState.bsteerEventsHandle = NULL;

    status |=
        wlanifLinkEventsDestroy(wlanifState.linkEventsHandle);
    wlanifState.linkEventsHandle = NULL;

    status |=
        wlanifBSteerControlDestroy(wlanifState.bsteerControlHandle);
    wlanifState.bsteerControlHandle = NULL;

    if(fd_lock >= 0) {
        flock(fd_lock, LOCK_UN);
        close(fd_lock);
    }
    return status;
}

LBD_STATUS wlanif_getRadioAddrFromBSS(const lbd_bssInfo_t *bss, struct ether_addr *radioAddr) {
    if (!radioAddr || !bss) return LBD_NOK;

    if (!wlanifState.bsteerControlHandle) {
        return LBD_NOK;
    }
    return wlanifBSteerControlCmnGetRadioAddrFromBSS(wlanifState.bsteerControlHandle, bss,
                                                     radioAddr);
}

LBD_STATUS wlanif_sendBcnRequestExtended(const struct ether_addr *staAddr,
                                         const wlanif_bcnMetricsQuery_t *query,
                                         const lbd_bssInfo_t *bss,
                                         u_int8_t clientClassGroup,
                                         LBD_BOOL useBeaconTable) {
    if (!staAddr || !query || !wlanifState.bsteerControlHandle || !bss) return LBD_NOK;

    return wlanifBSteerControlCmnSendBcnRequestExtended(wlanifState.bsteerControlHandle,
                                                        staAddr, query, bss,
                                                        clientClassGroup, useBeaconTable);
}
