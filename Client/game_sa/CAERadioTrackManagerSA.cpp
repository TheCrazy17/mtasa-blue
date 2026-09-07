/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto v1.0
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        game_sa/CAERadioTrackManagerSA.cpp
 *  PURPOSE:     Audio entity radio track manager
 *
 *  Multi Theft Auto is available from https://www.multitheftauto.com/
 *
 *****************************************************************************/

#include "StdInc.h"
#include "CAERadioTrackManagerSA.h"

BYTE CAERadioTrackManagerSA::GetCurrentRadioStationID()
{
    DWORD dwFunc = FUNC_GetCurrentRadioStationID;
    BYTE  bReturn = 0;
    // clang-format off
    __asm
    {
        mov     ecx, CLASS_CAERadioTrackManager
        call    dwFunc
        mov     bReturn, al
    }
    // clang-format on

    return bReturn;
}

BYTE CAERadioTrackManagerSA::IsVehicleRadioActive()
{
    DWORD dwFunc = FUNC_IsVehicleRadioActive;
    BYTE  bReturn = 0;
    // clang-format off
    __asm
    {
        mov     ecx, CLASS_CAERadioTrackManager
        call    dwFunc
        mov     bReturn, al
    }
    // clang-format on

    return bReturn;
}

char* CAERadioTrackManagerSA::GetRadioStationName(BYTE bStationID)
{
    DWORD dwFunc = FUNC_GetRadioStationName;
    char* cReturn = 0;
    DWORD dwStationID = bStationID;
    // clang-format off
    __asm
    {
        mov     ecx, CLASS_CAERadioTrackManager
        push    dwStationID
        call    dwFunc
        mov     cReturn, eax
    }
    // clang-format on

    return cReturn;
}

bool CAERadioTrackManagerSA::IsRadioOn()
{
    DWORD dwFunc = FUNC_IsRadioOn;
    bool  bReturn = false;
    // clang-format off
    __asm
    {
        mov     ecx, CLASS_CAERadioTrackManager
        call    dwFunc
        mov     bReturn, al
    }
    // clang-format on

    return bReturn;
}

void CAERadioTrackManagerSA::SetBassSetting(DWORD dwBass)
{
    DWORD dwFunc = FUNC_SetBassSetting;
    // clang-format off
    __asm
    {
        mov     ecx, CLASS_CAERadioTrackManager
        push    0x3F800000 // 1.0f
        push    dwBass
        call    dwFunc
    }
    // clang-format on
}

void CAERadioTrackManagerSA::Reset()
{
    DWORD dwFunc = FUNC_Reset;
    // clang-format off
    __asm
    {
        mov     ecx, CLASS_CAERadioTrackManager
        call    dwFunc
    }
    // clang-format on
}

void CAERadioTrackManagerSA::StartRadio(BYTE bStationID, BYTE bUnknown)
{
    DWORD dwFunc = FUNC_StartRadio;
    DWORD dwStationID = bStationID;
    DWORD dwUnknown = bUnknown;
    // clang-format off
    __asm
    {
        mov     ecx, CLASS_CAERadioTrackManager
        push    0
        push    0
        push    dwUnknown
        push    dwStationID
        call    dwFunc
    }
    // clang-format on
}

bool CAERadioTrackManagerSA::IsStationLoading() const
{
    CAERadioTrackManagerSAInterface* trackInterface = GetInterface();
    return (trackInterface->stationsListed || trackInterface->stationsListDown);
}

// Total number of radio stations the native engine knows about (RADIO_OFF..RADIO_USER_TRACKS
// inclusive), i.e. gta-reversed's eRadioID::RADIO_COUNT. Matches CAERadioTrackManagerSAInterface's
// own tracksInARow/listenItems/radioState array sizes.
constexpr std::uint8_t RADIO_STATION_COUNT = 14;

// Snapshots CAERadioTrackManager::m_ActiveSettings, the state Service() (0x4EB9A0) keeps
// continuously refreshed from the audio hardware every tick (PlayTime and CurrTrackID in
// particular), i.e. what is actually audible right now - not m_RequestedSettings, which is
// only a pending target until Service() copies it over.
bool CAERadioTrackManagerSA::GetPlaybackState(SRadioPlaybackState& outState)
{
    CAERadioTrackManagerSAInterface* trackInterface = GetInterface();
    const tRadioSettings&            active = trackInterface->activeSettings;

    outState.station = active.currentRadioStation;
    outState.mode = static_cast<std::uint8_t>(trackInterface->trackMode);
    outState.currentTrackID = active.currentTrackId;
    outState.currentTrackType = active.currentTrackType;
    outState.currentTrackIndex = active.currentTrackIndex;
    outState.playTime = active.trackPlayTime;
    outState.trackLength = active.trackLengthInMS;
    outState.flags = active.trackFlags;

    for (std::size_t i = 0; i < SRadioPlaybackState::QUEUE_SIZE; i++)
    {
        outState.queue[i].trackID = active.trackQueue[i];
        outState.queue[i].trackType = active.trackTypes[i];
        outState.queue[i].trackIndex = active.trackIndexes[i];
    }

    return true;
}

// Feeds a given state back into CAERadioTrackManager::m_RequestedSettings, the exact same
// hand-off point CAERadioTrackManager::StartRadio (0x4EB3C0) itself writes into, then arms
// m_bInitialised so Service()'s own STOPPED -> STARTING reset block picks it up on its own
// next tick - no new state machine, just feeding the real one the same way a fresh StartRadio
// call would. If something is already starting/waiting/playing, nudge trackMode to STOPPING
// first (again mirroring StartRadio exactly) so the hardware channel gets released cleanly
// before the restored track queue is handed to PlayTrack; otherwise leave whatever stop-in-
// progress mode is already running alone, since Service() will still reach STOPPED on its own.
bool CAERadioTrackManagerSA::SetPlaybackState(const SRadioPlaybackState& state)
{
    if (state.station >= RADIO_STATION_COUNT)
        return false;

    CAERadioTrackManagerSAInterface* trackInterface = GetInterface();
    tRadioSettings&                  requested = trackInterface->requestedSettings;

    requested.currentRadioStation = state.station;
    requested.currentTrackId = state.currentTrackID;
    requested.currentTrackType = static_cast<std::uint8_t>(state.currentTrackType);
    requested.currentTrackIndex = static_cast<std::int8_t>(state.currentTrackIndex);
    requested.trackPlayTime = state.playTime;
    requested.trackLengthInMS = state.trackLength;
    requested.trackFlags = state.flags;

    for (std::size_t i = 0; i < SRadioPlaybackState::QUEUE_SIZE; i++)
    {
        requested.trackQueue[i] = state.queue[i].trackID;
        requested.trackTypes[i] = static_cast<std::uint8_t>(state.queue[i].trackType);
        requested.trackIndexes[i] = static_cast<std::int8_t>(state.queue[i].trackIndex);
    }

    switch (trackInterface->trackMode)
    {
        case eRadioTrackMode::RADIO_STARTING:
        case eRadioTrackMode::RADIO_WAITING_TO_PLAY:
        case eRadioTrackMode::RADIO_PLAYING:
            trackInterface->trackMode = eRadioTrackMode::RADIO_STOPPING;
            break;
        default:
            break;
    }

    trackInterface->radioState[state.station].timeInPauseModeInMS = -1;
    trackInterface->isInitialised = true;
    return true;
}
