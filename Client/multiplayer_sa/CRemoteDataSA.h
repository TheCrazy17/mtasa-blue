/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto v1.0
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        multiplayer_sa/CRemoteDataSA.h
 *  PURPOSE:     Remote data storage class
 *
 *  Multi Theft Auto is available from https://www.multitheftauto.com/
 *
 *****************************************************************************/

#pragma once

#include "multiplayersa_init.h"
#include <multiplayer/CMultiplayer.h>

#include <game/CPlayerPed.h>
#include <game/CPad.h>

// These includes have to be fixed!
#include "../game_sa/CPadSA.h"

class CRemoteDataStorageSA : public CRemoteDataStorage
{
public:
    CRemoteDataStorageSA()
    {
        memset(&m_pad, 0, sizeof(CPadSAInterface));
        m_fCameraRotation = 0.0f;
        memset(&m_stats, 0, sizeof(CStatsData));
        m_fGravity = 0.008f;
        m_bAkimboTargetUp = false;
        m_bProcessPlayerWeapon = false;
        m_shotSyncData.m_bRemoteBulletSyncVectorsValid = false;
        m_bHydraulicsRaised = false;
        m_bHydraulicsRaisedKnown = false;
        m_pHydraulicsVehicle = nullptr;
    }

    CControllerState* CurrentControllerState() { return &m_pad.NewState; }
    CControllerState* LastControllerState() { return &m_pad.OldState; }
    CShotSyncData*    ShotSyncData() { return &m_shotSyncData; }
    CStatsData*       Stats() { return &m_stats; }
    void              SetCameraRotation(float fCameraRotation) { m_fCameraRotation = fCameraRotation; }
    float             GetCameraRotation() { return m_fCameraRotation; }
    void              SetGravity(float fGravity) { m_fGravity = fGravity; }
    bool              ProcessPlayerWeapon() { return m_bProcessPlayerWeapon; }
    void              SetProcessPlayerWeapon(bool bProcess) { m_bProcessPlayerWeapon = bProcess; }

    CVector& GetAkimboTarget() { return m_vecAkimboTarget; }
    bool     GetAkimboTargetUp() { return m_bAkimboTargetUp; }

    void SetAkimboTarget(const CVector& vecTarget) { m_vecAkimboTarget = vecTarget; }
    void SetAkimboTargetUp(bool bUp) { m_bAkimboTargetUp = bUp; }

    // Driver's hydraulics raised state, synced as a level rather than the horn press that toggles
    // it; replayed edges can drift over unreliable keysync, while a level self corrects.
    bool GetHydraulicsRaised() { return m_bHydraulicsRaised; }
    void SetHydraulicsRaised(bool bRaised)
    {
        m_bHydraulicsRaised = bRaised;
        m_bHydraulicsRaisedKnown = true;
    }

    // Treats the raised state as unknown until this driver has sent a keysync for this specific
    // vehicle, so a default or stale value from an earlier vehicle is never applied to a new one.
    bool RefreshHydraulicsVehicle(void* pVehicle)
    {
        if (m_pHydraulicsVehicle != pVehicle)
        {
            m_pHydraulicsVehicle = pVehicle;
            m_bHydraulicsRaisedKnown = false;
        }

        return m_bHydraulicsRaisedKnown;
    }

    // The player's pad
    CPadSAInterface m_pad;
    float           m_fCameraRotation;
    CShotSyncData   m_shotSyncData;
    CVector         m_vecAkimboTarget;
    bool            m_bAkimboTargetUp;
    CStatsData      m_stats;
    float           m_fGravity;
    bool            m_bProcessPlayerWeapon;
    bool            m_bHydraulicsRaised;
    bool            m_bHydraulicsRaisedKnown;
    void*           m_pHydraulicsVehicle;
};

class CRemoteDataSA
{
public:
    static void Init();

    // Static accessors
    static CRemoteDataStorageSA* GetRemoteDataStorage(CPlayerPed* pPed);
    static CRemoteDataStorageSA* GetRemoteDataStorage(CPedSAInterface* pPed);
    static void                  AddRemoteDataStorage(CPlayerPed* pPed, CRemoteDataStorage* pData);
    static void                  RemoveRemoteDataStorage(CPlayerPed* pPed);

private:
    static CPools*                                      m_pPools;
    static std::map<CPlayerPed*, CRemoteDataStorageSA*> m_RemoteData;
};
