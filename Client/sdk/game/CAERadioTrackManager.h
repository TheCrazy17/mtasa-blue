/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto v1.0
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        sdk/game/CAERadioTrackManager.h
 *  PURPOSE:     Radio track audio entity interface
 *
 *  Multi Theft Auto is available from https://www.multitheftauto.com/
 *
 *****************************************************************************/

#pragma once

// One entry of the native radio track manager's 5-entry playback queue (a mix of upcoming
// intro/track/outro/advert/DJ-banter/ident/user-track ids, depending on trackType).
struct SRadioPlaybackQueueEntry
{
    int trackID = -1;       // Native track/sample id, or -1 if this queue slot is unused
    int trackType = 0;      // TYPE_INTRO/TYPE_TRACK/TYPE_OUTRO/... (see AERadioTrackManager)
    int trackIndex = -1;    // Index into the station's own track list, or -1 if not applicable
};

// Snapshot of the native radio engine's live playback state (CAERadioTrackManager::m_ActiveSettings
// plus its current mode), for GetPlaybackState/SetPlaybackState. Grounded in the real engine's own
// tRadioSettings fields, not an invented shape - a setter feeds this back in through the same
// RequestedSettings hand-off StartRadio itself uses, so the native engine picks it up on its own.
struct SRadioPlaybackState
{
    static constexpr size_t QUEUE_SIZE = 5;

    unsigned char station = 0;                  // eRadioID station, RADIO_OFF(13)..RADIO_USER_TRACKS(12)
    unsigned char mode = 0;                      // eRadioTrackMode the state was captured in (informational)
    int           currentTrackID = -1;
    int           currentTrackType = 0;
    int           currentTrackIndex = -1;
    int           playTime = 0;                  // Milliseconds into the current track
    int           trackLength = 0;                // Current track's length in milliseconds
    unsigned char flags = 0;

    SRadioPlaybackQueueEntry queue[QUEUE_SIZE];
};

class CAERadioTrackManager
{
public:
    virtual BYTE  GetCurrentRadioStationID() = 0;
    virtual BYTE  IsVehicleRadioActive() = 0;
    virtual char* GetRadioStationName(BYTE bStationID) = 0;
    virtual bool  IsRadioOn() = 0;
    virtual void  SetBassSetting(DWORD dwBass) = 0;
    virtual void  Reset() = 0;
    virtual void  StartRadio(BYTE bStationID, BYTE bUnknown) = 0;
    virtual bool  IsStationLoading() const = 0;

    virtual bool GetPlaybackState(SRadioPlaybackState& outState) = 0;
    virtual bool SetPlaybackState(const SRadioPlaybackState& state) = 0;
};
