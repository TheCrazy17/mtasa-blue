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
// CTrailer::PreRender, spinning the farm trailer's reel on custom vehicle models
//
// The reel only turns for model 610, the stock farm trailer. A model created by
// engineRequestModel carries an ID of its own, so a cloned one never reaches the code that spins it
// with the trailer's forward speed.
//
//////////////////////////////////////////////////////////////////////////////////////////
// >>> 0x6CFC41 | 66 81 7E 22 62 02          | cmp     word ptr [esi + 0x22], 0x262
// >>> 0x6CFC47 | 0F 85 E8 00 00 00          | jne     0x6CFD35
//     0x6CFC4D | D9 86 E8 09 00 00          | fld     dword ptr [esi + 0x9E8]
#define HOOKPOS_CTrailer__PreRender_FarmReel  0x6CFC41
#define HOOKSIZE_CTrailer__PreRender_FarmReel 12
static const DWORD CONTINUE_CTrailer__PreRender_FarmReel = 0x6CFC4D;
static const DWORD SKIP_CTrailer__PreRender_FarmReel = 0x6CFD35;

static bool __fastcall IsFarmTrailerOrClone(CVehicleSAInterface* vehicle)
{
    const std::uint32_t modelId = vehicle->m_nModelIndex;
    if (modelId == static_cast<std::uint32_t>(VehicleType::VT_FARMTR1))
        return true;

    CModelInfo* modelInfo = pGameInterface->GetModelInfo(modelId);
    return modelInfo && modelInfo->GetParentID() == static_cast<unsigned int>(VehicleType::VT_FARMTR1);
}

static void __declspec(naked) HOOK_CTrailer__PreRender_FarmReel()
{
    MTA_VERIFY_HOOK_LOCAL_SIZE;

    // clang-format off
    __asm
    {
        pushad
        mov     ecx, esi
        call    IsFarmTrailerOrClone
        test    al, al
        popad
        jz      notFarmTrailer

        jmp     CONTINUE_CTrailer__PreRender_FarmReel

        notFarmTrailer:
        jmp     SKIP_CTrailer__PreRender_FarmReel
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
    EZHookInstall(CTrailer__PreRender_FarmReel);
}
