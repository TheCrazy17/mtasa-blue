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
    EZHookInstall(CPed_Destructor_ReleaseDuckColModel);
}
