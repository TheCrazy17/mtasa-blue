/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto v1.0
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        game_sa/CAERadioTrackManagerSA.cpp
 *  PURPOSE:     Header file for audio entity radio track manager class
 *
 *  Multi Theft Auto is available from https://www.multitheftauto.com/
 *
 *****************************************************************************/

#pragma once

#include <game/CAERadioTrackManager.h>

#define FUNC_GetCurrentRadioStationID 0x4E83F0
#define FUNC_IsVehicleRadioActive     0x4E9800
#define FUNC_GetRadioStationName      0x4E9E10
#define FUNC_IsRadioOn                0x4E8350
#define FUNC_SetBassSetting           0x4E82F0
#define FUNC_Reset                    0x4E7F80
#define FUNC_StartRadio               0x4EB3C0

#define CLASS_CAERadioTrackManager 0x8CB6F8

enum class eRadioTrackMode
{
    RADIO_STARTING,
    RADIO_WAITING_TO_PLAY,
    RADIO_PLAYING,
    RADIO_STOPPING,
    RADIO_STOPPING_SILENCED,
    RADIO_STOPPING_CHANNELS_STOPPED,
    RADIO_WAITING_TO_STOP,
    RADIO_STOPPED
};

struct tRadioSettings
{
    std::int32_t trackQueue[5];        // Up to 5 queued track/DJ-banter/advert/ident ids, played in order
    std::int32_t currentTrackId;
    std::int32_t prevTrackId;
    std::int32_t trackPlayTime;
    std::int32_t trackLengthInMS;
    std::uint8_t trackFlags;
    std::uint8_t currentRadioStation;
    std::uint8_t bassSet;
    float        bassGain;
    std::uint8_t trackTypes[5];        // Per-entry type (intro/track/outro/advert/...) for trackQueue
    std::uint8_t currentTrackType;
    std::uint8_t prevTrackType;
    std::int8_t  trackIndexes[5];      // Per-entry index into the station's track list, for trackQueue
    std::int8_t  currentTrackIndex;
    std::int8_t  prevTrackIndex;
};
static_assert(sizeof(tRadioSettings) == 0x3C, "Invalid size of tRadioSettings struct!");
// Sub-field offsets confirmed against real disassembly (Ghidra, gta_sa.exe 1.0 US) of
// CAERadioTrackManager::Service/StartRadio/CheckForTrackConcatenation/SetBassSetting/
// SetBassEnhanceOnOff - pinned here so the layout can't silently drift again like it did
// before (trackQueue was 4 entries instead of 5, trackFlags/trackIndexes were sized wrong,
// shifting every field's real offset without the overall struct size changing).
static_assert(offsetof(tRadioSettings, trackQueue) == 0x00, "Invalid offset of tRadioSettings::trackQueue!");
static_assert(offsetof(tRadioSettings, currentTrackId) == 0x14, "Invalid offset of tRadioSettings::currentTrackId!");
static_assert(offsetof(tRadioSettings, prevTrackId) == 0x18, "Invalid offset of tRadioSettings::prevTrackId!");
static_assert(offsetof(tRadioSettings, trackPlayTime) == 0x1C, "Invalid offset of tRadioSettings::trackPlayTime!");
static_assert(offsetof(tRadioSettings, trackLengthInMS) == 0x20, "Invalid offset of tRadioSettings::trackLengthInMS!");
static_assert(offsetof(tRadioSettings, trackFlags) == 0x24, "Invalid offset of tRadioSettings::trackFlags!");
static_assert(offsetof(tRadioSettings, currentRadioStation) == 0x25, "Invalid offset of tRadioSettings::currentRadioStation!");
static_assert(offsetof(tRadioSettings, bassSet) == 0x26, "Invalid offset of tRadioSettings::bassSet!");
static_assert(offsetof(tRadioSettings, bassGain) == 0x28, "Invalid offset of tRadioSettings::bassGain!");
static_assert(offsetof(tRadioSettings, trackTypes) == 0x2C, "Invalid offset of tRadioSettings::trackTypes!");
static_assert(offsetof(tRadioSettings, currentTrackType) == 0x31, "Invalid offset of tRadioSettings::currentTrackType!");
static_assert(offsetof(tRadioSettings, prevTrackType) == 0x32, "Invalid offset of tRadioSettings::prevTrackType!");
static_assert(offsetof(tRadioSettings, trackIndexes) == 0x33, "Invalid offset of tRadioSettings::trackIndexes!");
static_assert(offsetof(tRadioSettings, currentTrackIndex) == 0x38, "Invalid offset of tRadioSettings::currentTrackIndex!");
static_assert(offsetof(tRadioSettings, prevTrackIndex) == 0x39, "Invalid offset of tRadioSettings::prevTrackIndex!");

struct tRadioState
{
    std::int32_t elapsed[3];
    std::int32_t timeInPauseModeInMS;
    std::int32_t timeInMS;
    std::int32_t trackPlayTime;
    std::int32_t trackQueue[3];
    std::uint8_t trackTypes[3];
    std::uint8_t gameMonthDay;
    std::uint8_t gameClockHours;
};
static_assert(sizeof(tRadioState) == 0x2C, "Invalid size of tRadioState struct!");

class CAERadioTrackManagerSAInterface
{
public:
    bool            isInitialised;
    bool            displayStationName;
    std::uint8_t    field_2;
    bool            enableInPauseMode;
    bool            bassEnhance;
    bool            pauseMode;
    bool            retuneJustStarted;
    bool            autoSelect;
    std::uint8_t    tracksInARow[14];
    std::uint8_t    gameMonthDay;
    std::uint8_t    gameClockHours;
    std::int32_t    listenItems[14];
    std::uint32_t   timeRadioStationReturned;
    std::uint32_t   timeToDisplayRadioName;
    std::uint32_t   savedTimeInMS;
    std::uint32_t   retuneStartedTime;
    std::uint8_t    field_60[4];
    std::int32_t    hwClientHandle;
    eRadioTrackMode trackMode;
    std::int32_t    stationsListed;
    std::int32_t    stationsListDown;
    std::int32_t    savedRadioStationId;
    std::int32_t    radioStationMenuRequest;
    std::int32_t    radioStationScriptRequest;
    float           volume1;
    float           volume2;
    tRadioSettings  requestedSettings;
    tRadioSettings  activeSettings;
    tRadioState     radioState[14];    // One entry per eRadioID station (RADIO_COUNT), was wrongly [13]
    std::uint32_t   field_368;
    std::uint8_t    userTrackPlayMode;
    std::uint8_t    field_36D[3];
};
static_assert(sizeof(CAERadioTrackManagerSAInterface) == 0x370, "Invalid size of CAERadioTrackManagerSAInterface class!");
// Pin the fields around the fixed radioState[13] -> [14] bug (was quietly compensated for by
// two now-removed padding arrays, so the total size below already passed even when broken).
static_assert(offsetof(CAERadioTrackManagerSAInterface, trackMode) == 0x68, "Invalid offset of CAERadioTrackManagerSAInterface::trackMode!");
static_assert(offsetof(CAERadioTrackManagerSAInterface, stationsListed) == 0x6C, "Invalid offset of CAERadioTrackManagerSAInterface::stationsListed!");
static_assert(offsetof(CAERadioTrackManagerSAInterface, stationsListDown) == 0x70, "Invalid offset of CAERadioTrackManagerSAInterface::stationsListDown!");
static_assert(offsetof(CAERadioTrackManagerSAInterface, requestedSettings) == 0x88, "Invalid offset of CAERadioTrackManagerSAInterface::requestedSettings!");
static_assert(offsetof(CAERadioTrackManagerSAInterface, activeSettings) == 0xC4, "Invalid offset of CAERadioTrackManagerSAInterface::activeSettings!");
static_assert(offsetof(CAERadioTrackManagerSAInterface, radioState) == 0x100, "Invalid offset of CAERadioTrackManagerSAInterface::radioState!");
static_assert(offsetof(CAERadioTrackManagerSAInterface, field_368) == 0x368, "Invalid offset of CAERadioTrackManagerSAInterface::field_368!");
static_assert(offsetof(CAERadioTrackManagerSAInterface, userTrackPlayMode) == 0x36C, "Invalid offset of CAERadioTrackManagerSAInterface::userTrackPlayMode!");

class CAERadioTrackManagerSA : public CAERadioTrackManager
{
public:
    CAERadioTrackManagerSAInterface* GetInterface() const noexcept { return reinterpret_cast<CAERadioTrackManagerSAInterface*>(CLASS_CAERadioTrackManager); }

    BYTE  GetCurrentRadioStationID();
    BYTE  IsVehicleRadioActive();
    char* GetRadioStationName(BYTE bStationID);
    bool  IsRadioOn();
    void  SetBassSetting(DWORD dwBass);
    void  Reset();
    void  StartRadio(BYTE bStationID, BYTE bUnknown);
    bool  IsStationLoading() const;
};
