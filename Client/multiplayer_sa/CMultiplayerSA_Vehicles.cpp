/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        multiplayer_sa/CMultiplayerSA_Vehicles.cpp
 *
 *  Multi Theft Auto is available from https://www.multitheftauto.com/
 *
 *****************************************************************************/

#include "StdInc.h"
#include <enums/VehicleType.h>

static bool __fastcall AreVehicleDoorsUndamageable(CVehicleSAInterface* vehicle)
{
    SClientEntity<CVehicleSA>* pair = pGameInterface->GetPools()->GetVehicle((DWORD*)vehicle);

    if (!pair)
        return false;

    return pair->pEntity->AreDoorsUndamageable();
}

//////////////////////////////////////////////////////////////////////////////////////////
//
// CDamageManager::ProgressDoorDamage
//
// This hook checks if our CVehicleSA instance prevents door damage
//
//////////////////////////////////////////////////////////////////////////////////////////
// >>> 0x6C2320 | 53             | push    ebx
//     0x6C2321 | 56             | push    esi
//     0x6C2322 | 0F B6 74 24 0C | movzx   esi, [esp + doorId]
//     0x6C2327 | 85 F6          | test    esi, esi
#define HOOKPOS_CDamageManager__ProgressDoorDamage  0x6C2320
#define HOOKSIZE_CDamageManager__ProgressDoorDamage 7
static DWORD CONTINUE_CDamageManager__ProgressDoorDamage = 0x6C2327;

static void __declspec(naked) HOOK_CDamageManager__ProgressDoorDamage()
{
    MTA_VERIFY_HOOK_LOCAL_SIZE;

    // clang-format off
    __asm
    {
        pushad
        mov     ecx, [esp + 08h]        // CAutomobileSAInterface*
        call    AreVehicleDoorsUndamageable
        test    al, al
        jz      continueGameCodeLocation

        popad
        mov     al, 0
        retn    8

        continueGameCodeLocation:
        popad
        push    ebx
        push    esi
        movzx   esi, [esp + 0Ch]
        jmp     CONTINUE_CDamageManager__ProgressDoorDamage
    }
    // clang-format on
}

//////////////////////////////////////////////////////////////////////////////////////////
//
// CAutomobile::ProcessControl, reaching ProcessHarvester on custom vehicle models
//
// ProcessControl only calls ProcessHarvester, which clears peds and objects the combine drives
// through, for model 532. A model created by engineRequestModel carries an ID of its own, so a
// cloned combine harvester never reaches it. ProcessHarvester itself takes no further model checks,
// so letting the clone through here is the whole fix.
//
//////////////////////////////////////////////////////////////////////////////////////////
// >>> 0x6B36C5 | 66 81 7E 22 14 02 | cmp     word ptr [esi + 0x22], 0x214
// >>> 0x6B36CB | 75 07             | jne     0x6B36D4
//     0x6B36CD | 8B CE             | mov     ecx, esi
#define HOOKPOS_CAutomobile__ProcessControl_HarvesterDispatch  0x6B36C5
#define HOOKSIZE_CAutomobile__ProcessControl_HarvesterDispatch 8
static const DWORD CONTINUE_CAutomobile__ProcessControl_HarvesterDispatch = 0x6B36CD;
static const DWORD SKIP_CAutomobile__ProcessControl_HarvesterDispatch = 0x6B36D4;

static bool __fastcall IsCombineHarvesterOrClone(CVehicleSAInterface* vehicle)
{
    const std::uint32_t modelId = vehicle->m_nModelIndex;
    if (modelId == static_cast<std::uint32_t>(VehicleType::VT_COMBINE))
        return true;

    CModelInfo* modelInfo = pGameInterface->GetModelInfo(modelId);
    return modelInfo && modelInfo->GetParentID() == static_cast<unsigned int>(VehicleType::VT_COMBINE);
}

static void __declspec(naked) HOOK_CAutomobile__ProcessControl_HarvesterDispatch()
{
    MTA_VERIFY_HOOK_LOCAL_SIZE;

    // clang-format off
    __asm
    {
        pushad
        mov     ecx, esi
        call    IsCombineHarvesterOrClone
        test    al, al
        popad
        jz      notCombine

        jmp     CONTINUE_CAutomobile__ProcessControl_HarvesterDispatch

        notCombine:
        jmp     SKIP_CAutomobile__ProcessControl_HarvesterDispatch
    }
    // clang-format on
}

//////////////////////////////////////////////////////////////////////////////////////////
//
// CMultiplayerSA::InitHooks_Vehicles
//
// Setup hooks
//
//////////////////////////////////////////////////////////////////////////////////////////
void CMultiplayerSA::InitHooks_Vehicles()
{
    EZHookInstall(CDamageManager__ProgressDoorDamage);
    EZHookInstall(CAutomobile__ProcessControl_HarvesterDispatch);
}
