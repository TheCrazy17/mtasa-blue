/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto v1.0
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        mods/deathmatch/logic/CTrailerLinkHelper.cpp
 *  PURPOSE:     Shared trailer attach/detach logic
 *
 *  Multi Theft Auto is available from https://www.multitheftauto.com/
 *
 *****************************************************************************/

#include "StdInc.h"
#include "CTrailerLinkHelper.h"
#include "CGame.h"
#include "packets/CVehicleTrailerPacket.h"

extern CGame* g_pGame;

void CTrailerLinkHelper::DetachTrailer(CVehicle* pVehicle, CVehicle* pTrailer, CPlayer* pSourcePlayer)
{
    CVehicle* pCurrentTrailer = pVehicle->GetTowedVehicle();
    if (!pCurrentTrailer || (pTrailer && pCurrentTrailer != pTrailer))
        return;

    pVehicle->SetTowedVehicle(NULL);

    CVehicleTrailerPacket DetachPacket(pVehicle, pCurrentTrailer, false);
    g_pGame->GetPlayerManager()->BroadcastOnlyJoined(DetachPacket, pSourcePlayer);

    CLuaArguments Arguments;
    Arguments.PushElement(pVehicle);
    pCurrentTrailer->CallEvent("onTrailerDetach", Arguments);
}

bool CTrailerLinkHelper::AttachTrailer(CVehicle* pVehicle, CVehicle* pTrailer, CPlayer* pSourcePlayer)
{
    if (pVehicle->GetTowedVehicle() == pTrailer)
        return true;

    // A sync driven retry of a pair a script just cancelled is stale data sent before that
    // client saw the rollback, and would fire onTrailerAttach a second time; only scripted
    // attaches may try again immediately
    if (pSourcePlayer && pVehicle->WasTrailerRejectedRecently(pTrailer))
        return true;

    // Detach whatever either side is currently linked to first
    DetachTrailer(pVehicle, nullptr, pSourcePlayer);
    if (pVehicle->IsBeingDeleted() || pTrailer->IsBeingDeleted())
        return false;

    if (CVehicle* pCurrentTower = pTrailer->GetTowedByVehicle())
    {
        DetachTrailer(pCurrentTower, pTrailer, pSourcePlayer);
        if (pVehicle->IsBeingDeleted() || pTrailer->IsBeingDeleted())
            return false;
    }

    // The setter maintains both directions and refuses a circular chain
    if (!pVehicle->SetTowedVehicle(pTrailer))
        return false;

    CVehicleTrailerPacket AttachPacket(pVehicle, pTrailer, true);
    g_pGame->GetPlayerManager()->BroadcastOnlyJoined(AttachPacket, pSourcePlayer);

    CLuaArguments Arguments;
    Arguments.PushElement(pVehicle);
    bool bContinue = pTrailer->CallEvent("onTrailerAttach", Arguments);
    if (pVehicle->IsBeingDeleted() || pTrailer->IsBeingDeleted())
        return false;

    if (!bContinue)
    {
        pVehicle->SetTowedVehicle(NULL);
        pVehicle->SetRejectedTrailer(pTrailer);

        // No source skip here; the reporting client committed locally and needs the rollback
        CVehicleTrailerPacket DetachPacket(pVehicle, pTrailer, false);
        g_pGame->GetPlayerManager()->BroadcastOnlyJoined(DetachPacket);
    }

    return true;
}
