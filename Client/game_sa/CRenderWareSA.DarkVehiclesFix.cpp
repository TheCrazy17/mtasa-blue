/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto v1.0
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        game_sa/CRenderWareSA.DarkVehiclesFix.cpp
 *  PURPOSE:     Restore blown-up vehicles' dark "scorched" render
 *
 *  Multi Theft Auto is available from https://www.multitheftauto.com/
 *
 *  CAutomobile::BlowUpCar sets ATOMIC_PIPE_NO_EXTRA_PASSES (0x4000) on the wreck's
 *  clump to kill the specular/shine passes, but the stock PC custom car env-map
 *  pipeline (CCustomCarEnvMapPipeline::CustomPipeRenderCB) never tests that flag,
 *  so wrecks stay shiny instead of going flat black like on PS2. Credit to
 *  SilentPatch ("DarkVehiclesFix") for discovering and fixing this PC port
 *  regression; this is an independent reimplementation of the same hooks.
 *
 *****************************************************************************/

#include "StdInc.h"
#include "CGameSA.h"

// Address of the game's current main directional light pointer (RpLight*).
// Found via the operand of `mov dword ptr [ADDR], eax` at 0x5BA572, where the
// light is created and assigned.
#define ADDR_CurrentDirectionalLight 0xC886EC

// rpLIGHTLIGHTATOMICS: RenderWare light flag enabling lighting of atomics.
// Scorched-render passes run with this flag off on the directional light.
#define RW_LIGHT_FLAG_LIGHTATOMICS 1

// ATOMIC_PIPE_NO_EXTRA_PASSES: set by CAutomobile::BlowUpCar on blown-up vehicles
// to request skipping the specular/shine passes (bit 14 of the atomic pipe flags).
#define ATOMIC_PIPE_NO_EXTRA_PASSES_BIT 0xE

// Set by HOOK_DarkVehiclesFix1, read by the other three hooks within the same
// CustomPipeRenderCB call, so it must not be reset between them.
static bool ms_bBlownVehicleDarkRender = false;

//////////////////////////////////////////////////////////////////////////////////////////
//
// DarkVehiclesFix1
//
// Replaces a call inside CCustomCarEnvMapPipeline::CustomPipeRenderCB. Decides whether the
// current atomic is a blown-up vehicle being rendered without atom lighting, then performs
// the original call before resuming.
//
//////////////////////////////////////////////////////////////////////////////////////////
#define HOOKPOS_DarkVehiclesFix1  0x5D993F
#define HOOKSIZE_DarkVehiclesFix1 5
DWORD                         RETURN_DarkVehiclesFix1 = 0x5D9944;
static void __declspec(naked) HOOK_DarkVehiclesFix1()
{
    MTA_VERIFY_HOOK_LOCAL_SIZE;

    // clang-format off
    __asm
    {
        // eax = atomic pipe flags, untouched since entry to CustomPipeRenderCB
        shr     eax, ATOMIC_PIPE_NO_EXTRA_PASSES_BIT
        test    al, 1
        jz      DontApply

        mov     ecx, ADDR_CurrentDirectionalLight
        mov     ecx, [ecx]
        mov     al, [ecx+2]
        test    al, RW_LIGHT_FLAG_LIGHTATOMICS
        jnz     DontApply

        mov     ms_bBlownVehicleDarkRender, 1
        jmp     Continue

    DontApply:
        mov     ms_bBlownVehicleDarkRender, 0

    Continue:
        mov     edx, 0x756D90
        call    edx
        jmp     RETURN_DarkVehiclesFix1
    }
    // clang-format on
}

//////////////////////////////////////////////////////////////////////////////////////////
//
// DarkVehiclesFix2/3/4
//
// Each replaces a conditional jump that decides whether to run a pass (specular, shine-cam,
// shine-wave). When the wreck should render dark, force the "skip pass" branch even if the
// game's own check would otherwise have run the pass.
//
//////////////////////////////////////////////////////////////////////////////////////////
#define HOOKPOS_DarkVehiclesFix2  0x5D9A74
#define HOOKSIZE_DarkVehiclesFix2 6
static void __declspec(naked) HOOK_DarkVehiclesFix2()
{
    MTA_VERIFY_HOOK_LOCAL_SIZE;

    // clang-format off
    __asm
    {
        jz      MakeItDark
        mov     al, ms_bBlownVehicleDarkRender
        test    al, al
        jnz     MakeItDark
        mov     eax, 0x5D9A7A
        jmp     eax

    MakeItDark:
        mov     eax, 0x5D9B09
        jmp     eax
    }
    // clang-format on
}

#define HOOKPOS_DarkVehiclesFix3  0x5D9B44
#define HOOKSIZE_DarkVehiclesFix3 6
static void __declspec(naked) HOOK_DarkVehiclesFix3()
{
    MTA_VERIFY_HOOK_LOCAL_SIZE;

    // clang-format off
    __asm
    {
        jz      MakeItDark
        mov     al, ms_bBlownVehicleDarkRender
        test    al, al
        jnz     MakeItDark
        mov     eax, 0x5D9B4A
        jmp     eax

    MakeItDark:
        mov     eax, 0x5D9CAC
        jmp     eax
    }
    // clang-format on
}

#define HOOKPOS_DarkVehiclesFix4  0x5D9CB2
#define HOOKSIZE_DarkVehiclesFix4 6
static void __declspec(naked) HOOK_DarkVehiclesFix4()
{
    MTA_VERIFY_HOOK_LOCAL_SIZE;

    // clang-format off
    __asm
    {
        jz      MakeItDark
        mov     al, ms_bBlownVehicleDarkRender
        test    al, al
        jnz     MakeItDark
        mov     eax, 0x5D9CB8
        jmp     eax

    MakeItDark:
        mov     eax, 0x5D9E0D
        jmp     eax
    }
    // clang-format on
}

//////////////////////////////////////////////////////////////////////////////////////////
//
// Setup hooks
//
//////////////////////////////////////////////////////////////////////////////////////////
void CRenderWareSA::StaticSetDarkVehiclesHooks()
{
    EZHookInstall(DarkVehiclesFix1);
    EZHookInstall(DarkVehiclesFix2);
    EZHookInstall(DarkVehiclesFix3);
    EZHookInstall(DarkVehiclesFix4);
}
