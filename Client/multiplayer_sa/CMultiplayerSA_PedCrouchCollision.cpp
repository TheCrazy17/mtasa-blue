/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        multiplayer_sa/CMultiplayerSA_PedCrouchCollision.cpp
 *
 *  Multi Theft Auto is available from https://www.multitheftauto.com/
 *
 *****************************************************************************/
#include "StdInc.h"
#include "..\game_sa\CColModelSA.h"
#include <unordered_map>

//////////////////////////////////////////////////////////////////////////////////////////
//
// Ducking peds get a shrunk, private CColModel, so they can fit through gaps too low for
// their standing shape.
//
// Vanilla GTA:SA never shrinks a ducking ped's collision (TaskSimpleDuck is pure animation),
// but vehicles already have a per-instance collision override: CEntity::GetColModel (0x535300)
// returns CVehicle::m_aSpecialColModel[m_vehicleSpecialColIndex] when that index is set,
// instead of the model's shared CColModel. That mechanism is a fixed 4-slot pool baked into
// the retail exe for vehicle hydraulics (see CVehicle::GetSpecialColModel, 0x6DF3D0) - a
// pool that small would cap simultaneous crouching peds server-wide and would fight with
// hydraulics vehicles for the same slots, so it doesn't generalise directly. This mirrors
// its *pattern* instead: a per-instance override consulted at the same decision point, just
// backed by ordinary heap allocation (one per ducking ped, released the moment it stands),
// since peds have no equivalent native pool to draw from.
//
//////////////////////////////////////////////////////////////////////////////////////////

static std::unordered_map<CPedSAInterface*, CColModelSAInterface*> ms_PedDuckColModels;

// CModelInfo::ms_modelInfoPtrs. CBaseModelInfoSAInterface::pColModel sits at +0x14 in every
// model info type; read as a raw offset here (rather than pulling in CModelInfoSA.h) to match
// how the rest of multiplayer_sa already looks model info up (see CMultiplayerSA_Vehicles.cpp).
static const std::uint32_t CModelInfo__ms_modelInfoPtrs = 0xA9B0C8;

// Portion of standing height kept while ducking. GTA:SA never shrank ped collision for
// ducking, so there's no existing engine constant to source a number from - the camera's own
// duck height handling (CCamera::HandleCameraMotionForDucking, 0x50CFA0) is itself unreversed
// in gta-reversed. This uses the same stand:crouch ratio as CS 1.6 (72 -> 36 hull units),
// the reference point behind this feature request; tune after in-game testing.
constexpr float DUCK_HEIGHT_SCALE = 0.5f;

// Builds a private copy of a model's shared CColModel, shrunk to duck height.
//
// m_data is copied as a raw pointer, not deep-copied: it keeps pointing at the shared
// collision mesh. Peds don't carry meaningful per-triangle collision (their push-back test
// runs off the bound box/sphere only - see CPed::ProcessEntityCollision, 0x5E2530), and
// multiple ped instances of one model already share and scratch-mutate that same data each
// frame in retail gameplay, so sharing it here doesn't add any new risk. Only the box and
// sphere below get new (shrunk) values.
static CColModelSAInterface* CreatePedDuckColModel(CColModelSAInterface* pSharedColModel)
{
    auto* pOverride = new CColModelSAInterface(*pSharedColModel);

    const float fMinZ = pOverride->m_bounds.m_vecMin.fZ;
    const float fDuckHeight = (pOverride->m_bounds.m_vecMax.fZ - fMinZ) * DUCK_HEIGHT_SCALE;

    pOverride->m_bounds.m_vecMax.fZ = fMinZ + fDuckHeight;

    // Re-centre and shrink the bounding sphere to match, rather than scaling it in place -
    // the original sphere isn't guaranteed to be centred on the box's Z midpoint.
    pOverride->m_sphere.m_center.fZ = fMinZ + fDuckHeight * 0.5f;
    pOverride->m_sphere.m_radius *= DUCK_HEIGHT_SCALE;

    return pOverride;
}

static void ReleasePedDuckColModel(CPedSAInterface* pPed)
{
    auto iter = ms_PedDuckColModels.find(pPed);
    if (iter == ms_PedDuckColModels.end())
        return;

    delete iter->second;
    ms_PedDuckColModels.erase(iter);
}

// The single place that owns the override's lifetime, tied directly to the native bIsDucking
// flag: called from both the GetColModel hook (this ped as the *other* party in someone
// else's collision test) and the ProcessEntityCollision hook (this ped's *own* shape when
// it's the one being tested against the world) - both need the same answer.
static CColModelSAInterface* GetPedDuckColModelOverride(CPedSAInterface* pPed)
{
    if (!pPed->pedFlags.bIsDucking)
    {
        ReleasePedDuckColModel(pPed);
        return nullptr;
    }

    auto iter = ms_PedDuckColModels.find(pPed);
    if (iter != ms_PedDuckColModels.end())
        return iter->second;

    auto* pModelInfo = reinterpret_cast<std::uint8_t**>(CModelInfo__ms_modelInfoPtrs)[pPed->m_nModelIndex];
    if (!pModelInfo)
        return nullptr;

    auto* pSharedColModel = *reinterpret_cast<CColModelSAInterface**>(pModelInfo + 0x14);
    if (!pSharedColModel)
        return nullptr;

    auto* pOverride = CreatePedDuckColModel(pSharedColModel);
    ms_PedDuckColModels[pPed] = pOverride;
    return pOverride;
}

//////////////////////////////////////////////////////////////////////////////////////////
//
// CEntity::GetColModel
//
// Extends the same type-branch the retail exe already uses for the vehicle special-collision
// case, adding a ped case right beside it: a ducking ped with an active override returns
// that instead of falling through to the model's shared CColModel.
//
//////////////////////////////////////////////////////////////////////////////////////////
//     0x535300 | 8A 41 36 | mov al, [ecx+36h]      ; entity type & status byte
// >>> 0x535303 | 24 07    | and al, 7               ; al = entity type (PED == 3)
//     0x535305 | 3C 02    | cmp al, 2               ; ENTITY_TYPE_VEHICLE
#define HOOKPOS_CEntity_GetColModel  0x535300
#define HOOKSIZE_CEntity_GetColModel 5
static const DWORD RETURN_CEntity_GetColModel = 0x535305;

static void __declspec(naked) HOOK_CEntity_GetColModel()
{
    MTA_VERIFY_HOOK_LOCAL_SIZE;

    // clang-format off
    __asm
    {
        // Replicate the bytes this hook overwrites
        mov     al, [ecx+36h]
        and     al, 07h

        cmp     al, 3                   // ENTITY_TYPE_PED
        jnz     NotPed

        push    ecx                     // saved, restored below - callee is free to clobber ecx
        push    ecx                     // arg: CPedSAInterface*
        call    GetPedDuckColModelOverride
        add     esp, 4
        pop     ecx

        test    eax, eax
        jnz     UseOverride

        mov     al, 3                   // al was clobbered by the call; restore it for the fallthrough below
    NotPed:
        jmp     RETURN_CEntity_GetColModel

    UseOverride:
        ret                             // CColModel* already in eax; thiscall, no stack args to clean
    }
    // clang-format on
}

//////////////////////////////////////////////////////////////////////////////////////////
//
// CPed::ProcessEntityCollision
//
// The ped-vs-world push-back test (called once per nearby entity every physics tick) fetches
// its OWN col model directly from the model's shared CColModel, not through GetColModel() -
// it's the only path found this way, so hooking GetColModel alone would shrink a ducking
// ped's collision as seen by everyone else while leaving its own side of the test full size,
// meaning it would never actually fit through anything. This redirects that one fetch through
// the same override used above.
//
//////////////////////////////////////////////////////////////////////////////////////////
//     0x5E2570 | ...
// >>> 0x5E257A | 8B 68 14          | mov ebp, [eax+14h]      ; ebp = this ped's own CColModel*
// >>> 0x5E257D | 8B 86 74 04 00 00 | mov eax, [esi+474h]     ; unrelated flag read, only overwritten
//     0x5E2583 | 84 E4             | test ah, ah             ;   because it shares the 9-byte window
#define HOOKPOS_CPed_ProcessEntityCollision_OwnColModel  0x5E257A
#define HOOKSIZE_CPed_ProcessEntityCollision_OwnColModel 9
static const DWORD RETURN_CPed_ProcessEntityCollision_OwnColModel = 0x5E2583;

static void __declspec(naked) HOOK_CPed_ProcessEntityCollision_OwnColModel()
{
    MTA_VERIFY_HOOK_LOCAL_SIZE;

    // clang-format off
    __asm
    {
        // eax still holds this ped's CBaseModelInfoSAInterface* here (unchanged from the
        // original code at this point); esi is this ped (CPedSAInterface*, unaffected by the
        // call below - esi is callee-saved).
        push    eax                     // save the model info ptr for the no-override fallback
        push    esi                     // arg: CPedSAInterface*
        call    GetPedDuckColModelOverride
        add     esp, 4

        test    eax, eax
        jnz     UseOverride

        pop     eax
        mov     ebp, [eax+14h]          // original behaviour: this ped's shared CColModel
        jmp     Continue

    UseOverride:
        add     esp, 4                  // discard the saved model info ptr, not needed here
        mov     ebp, eax

    Continue:
        mov     eax, [esi+474h]         // replicate the second overwritten instruction
        jmp     RETURN_CPed_ProcessEntityCollision_OwnColModel
    }
    // clang-format on
}

//////////////////////////////////////////////////////////////////////////////////////////
//
// Stand-up obstruction test
//
// CPhysical::TestCollision (0x54DEC0) already does exactly what's needed here: it runs the
// engine's real collision-response pipeline (CheckCollision, the same one every physical
// object's movement goes through every tick) against the entity's current position without
// actually moving it, then restores everything it touched. Reused as-is rather than
// hand-rolling a second collision query, so this asks the same question the engine itself
// would ask - just with the duck shrink temporarily lifted, so it answers for the ped's real,
// standing-size shape instead of its current ducked one.
//
//////////////////////////////////////////////////////////////////////////////////////////
static bool IsPedStandUpBlocked(CPedSAInterface* pPed)
{
    // Never block a dying ped - the duck task's own health check needs to always go through;
    // death/ragdoll handling downstream doesn't expect the duck task to refuse to end.
    if (pPed->fHealth < 1.0f)
        return false;

    // Never had a shrunk override in the first place (this tick's clear is redundant, or the
    // ped was never actually ducking) - nothing to re-test against.
    if (ms_PedDuckColModels.find(pPed) == ms_PedDuckColModels.end())
        return false;

    const bool bWasDucking = pPed->pedFlags.bIsDucking;
    pPed->pedFlags.bIsDucking = false;  // GetPedDuckColModelOverride now returns null for this ped

    const bool bBlocked = reinterpret_cast<bool(__thiscall*)(CPedSAInterface*, bool)>(0x54DEC0)(pPed, false);

    pPed->pedFlags.bIsDucking = bWasDucking;

    return bBlocked;
}

//////////////////////////////////////////////////////////////////////////////////////////
//
// CTaskSimpleDuck::MakeAbortable
//
// Gates the one point where an ABORT_PRIORITY_URGENT abort (the duck timing out, or the
// player releasing the duck control) commits to ending the task, versus deferring it -
// deferring is already how this function refuses to end while a crouch-roll animation is
// mid-play, this adds the same refusal for "something is still overhead". Left untouched:
// ABORT_PRIORITY_IMMEDIATE (0x69210C, a few lines above this) always commits unconditionally
// in the retail exe - nothing here ever gets the chance to refuse it, and this doesn't add
// that ability, since a forced interrupt (entering a vehicle, a scripted task change, etc.)
// isn't safe to block on a collision test.
//
//////////////////////////////////////////////////////////////////////////////////////////
//     0x69231B | ...
// >>> 0x692321 | 83 FD 01 | cmp ebp, 1     ; ebp = priority; 1 == ABORT_PRIORITY_URGENT
// >>> 0x692326 | 75 09    | jnz +9         ; not urgent (e.g. LEISURE) -> defer (0x69232F)
//     0x692326 |          | (commit: mark finished, clear bIsDucking, return true)
//     0x69232F |          | (defer: mark aborting, return false)
#define HOOKPOS_CTaskSimpleDuck_MakeAbortable_StandCheck  0x692321
#define HOOKSIZE_CTaskSimpleDuck_MakeAbortable_StandCheck 5
static const DWORD RETURN_CTaskSimpleDuck_MakeAbortable_Commit = 0x692326;
static const DWORD RETURN_CTaskSimpleDuck_MakeAbortable_Defer = 0x69232F;

static void __declspec(naked) HOOK_CTaskSimpleDuck_MakeAbortable_StandCheck()
{
    MTA_VERIFY_HOOK_LOCAL_SIZE;

    // clang-format off
    __asm
    {
        cmp     ebp, 1
        jnz     Defer

        // Past the 3 prologue pushes (ebp/esi/edi) with the stack otherwise balanced here,
        // the ped argument sits at [esp+10h] - true on every path that reaches this point.
        mov     eax, [esp+10h]
        push    eax
        call    IsPedStandUpBlocked
        add     esp, 4
        test    al, al
        jnz     Defer

    Commit:
        jmp     RETURN_CTaskSimpleDuck_MakeAbortable_Commit
    Defer:
        jmp     RETURN_CTaskSimpleDuck_MakeAbortable_Defer
    }
    // clang-format on
}

//////////////////////////////////////////////////////////////////////////////////////////
//
// CTaskSimpleDuck::ProcessPed and CTaskSimpleDuck::MakeAbortable (shot-whizzing case)
//
// Two more places clear bIsDucking directly rather than going through the URGENT-commit
// branch above, so the hook there doesn't see them:
//
//  - ProcessPed's own finishing block (health <= 0, or m_bIsFinished already set - e.g. once
//    a LEISURE-priority toggle-off's blend-out animation finishes) calls MakeAbortable but
//    never looks at its return value, then clears the flag unconditionally right after.
//  - MakeAbortable's shot-whizzing special case (dodging gunfire while ducking) returns early
//    with its own copy of the same clear, before ever reaching the branch hooked above.
//
// Both boil down to the same instruction: `ped->flags &= ~0x04000000` (bIsDucking, bit 26).
// Gated the same way in both places: skip the clear (stay ducking) if standing is blocked.
//
//////////////////////////////////////////////////////////////////////////////////////////

static CPedSAInterface* g_pDuckClearCheckPed;

static bool ShouldSkipDuckClear()
{
    return IsPedStandUpBlocked(g_pDuckClearCheckPed);
}

//     0x694618 | ...
// >>> 0x69461F | 81 A7 6C 04 00 00 FF FF FF FB | and dword ptr [edi+46Ch], 0FBFFFFFFh   ; ped->bIsDucking = false
//     0x694629 | 5F                            | pop edi
#define HOOKPOS_CTaskSimpleDuck_ProcessPed_ClearDucking  0x69461F
#define HOOKSIZE_CTaskSimpleDuck_ProcessPed_ClearDucking 10
static const DWORD RETURN_CTaskSimpleDuck_ProcessPed_ClearDucking = 0x694629;

static void __declspec(naked) HOOK_CTaskSimpleDuck_ProcessPed_ClearDucking()
{
    MTA_VERIFY_HOOK_LOCAL_SIZE;

    // clang-format off
    __asm { mov g_pDuckClearCheckPed, edi }
    // clang-format on

    if (!ShouldSkipDuckClear())
    {
        // clang-format off
        __asm { and dword ptr [edi+46Ch], 0FBFFFFFFh }
        // clang-format on
    }

    // clang-format off
    __asm { jmp RETURN_CTaskSimpleDuck_ProcessPed_ClearDucking }
    // clang-format on
}

//     0x6921C7 | ...
//     0x6921CB | 8B 44 24 10                   | mov eax, [esp+10h]                     ; eax = ped
// >>> 0x6921CF | 81 A0 6C 04 00 00 FF FF FF FB | and dword ptr [eax+46Ch], 0FBFFFFFFh    ; ped->bIsDucking = false
//     0x6921D9 | 5F                            | pop edi
#define HOOKPOS_CTaskSimpleDuck_MakeAbortable_ShotWhizzClearDucking  0x6921CF
#define HOOKSIZE_CTaskSimpleDuck_MakeAbortable_ShotWhizzClearDucking 10
static const DWORD RETURN_CTaskSimpleDuck_MakeAbortable_ShotWhizzClearDucking = 0x6921D9;

static void __declspec(naked) HOOK_CTaskSimpleDuck_MakeAbortable_ShotWhizzClearDucking()
{
    MTA_VERIFY_HOOK_LOCAL_SIZE;

    // clang-format off
    __asm { mov g_pDuckClearCheckPed, eax }
    // clang-format on

    if (!ShouldSkipDuckClear())
    {
        // clang-format off
        __asm
        {
            mov     eax, g_pDuckClearCheckPed   // eax was clobbered by the call above, reload it
            and     dword ptr [eax+46Ch], 0FBFFFFFFh
        }
        // clang-format on
    }

    // clang-format off
    __asm { jmp RETURN_CTaskSimpleDuck_MakeAbortable_ShotWhizzClearDucking }
    // clang-format on
}

//////////////////////////////////////////////////////////////////////////////////////////
//
// CPed::~CPed
//
// Releases a ped's override, if it has one, before its interface memory can be freed and
// reused by a future, unrelated ped at the same address - without this, that future ped
// could inherit a stale override sized and shaped for a completely different ped.
//
//////////////////////////////////////////////////////////////////////////////////////////
//     0x5E8620 | 6A FF          | push -1
// >>> 0x5E8620 | E9 45 8D E1 FF | jmp 0x40136C     ; shared SEH prologue trampoline
//     0x5E8627 | ...            ; (resumes here once the trampoline has run)
#define HOOKPOS_CPed_Destructor_ReleaseDuckColModel  0x5E8620
#define HOOKSIZE_CPed_Destructor_ReleaseDuckColModel 7
static const DWORD SEH_PROLOG_TRAMPOLINE_CPed_Destructor = 0x40136C;

static void __declspec(naked) HOOK_CPed_Destructor_ReleaseDuckColModel()
{
    MTA_VERIFY_HOOK_LOCAL_SIZE;

    // clang-format off
    __asm
    {
        push    ecx                     // saved, restored below - callee is free to clobber ecx
        push    ecx                     // arg: CPedSAInterface* (this)
        call    ReleasePedDuckColModel
        add     esp, 4
        pop     ecx

        // Replicate the bytes this hook overwrites
        push    -1
        jmp     SEH_PROLOG_TRAMPOLINE_CPed_Destructor
    }
    // clang-format on
}

//////////////////////////////////////////////////////////////////////////////////////////
//
// CMultiplayerSA::InitHooks_PedCrouchCollision
//
// Setup hooks
//
//////////////////////////////////////////////////////////////////////////////////////////
void CMultiplayerSA::InitHooks_PedCrouchCollision()
{
    EZHookInstall(CEntity_GetColModel);
    EZHookInstall(CPed_ProcessEntityCollision_OwnColModel);
    EZHookInstall(CTaskSimpleDuck_MakeAbortable_StandCheck);
    EZHookInstall(CTaskSimpleDuck_ProcessPed_ClearDucking);
    EZHookInstall(CTaskSimpleDuck_MakeAbortable_ShotWhizzClearDucking);
    EZHookInstall(CPed_Destructor_ReleaseDuckColModel);
}
