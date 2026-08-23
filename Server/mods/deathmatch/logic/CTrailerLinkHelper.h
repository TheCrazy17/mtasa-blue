/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto v1.0
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        mods/deathmatch/logic/CTrailerLinkHelper.h
 *  PURPOSE:     Shared trailer attach/detach logic
 *
 *  Multi Theft Auto is available from https://www.multitheftauto.com/
 *
 *****************************************************************************/

#pragma once

class CVehicle;
class CPlayer;

// Single implementation for the sync, packet and scripting paths, which had each drifted.
// pSourcePlayer, when given, is the client that already committed the change locally and is
// skipped on the broadcast; a cancellation rollback always reaches it.
class CTrailerLinkHelper
{
public:
    // Clears any existing link on either side, then fires onTrailerAttach and rolls back on
    // cancel. Returns false when no link could be made: a script handler deleted either
    // vehicle mid call, or the pair would close a circular chain. A cancel still returns
    // true, so callers must recheck GetTowedVehicle before treating the link as real.
    static bool AttachTrailer(CVehicle* pVehicle, CVehicle* pTrailer, CPlayer* pSourcePlayer = nullptr);

    // Detaches pVehicle's trailer, or nothing unless pTrailer is the one attached
    static void DetachTrailer(CVehicle* pVehicle, CVehicle* pTrailer = nullptr, CPlayer* pSourcePlayer = nullptr);
};
