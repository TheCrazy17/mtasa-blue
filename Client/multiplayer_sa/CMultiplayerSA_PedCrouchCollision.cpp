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
#include <algorithm>
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

// A script's own explicit scale (setPedCollisionHeight/getPedCollisionHeight, Lua-facing),
// keyed the same way as the cache above. A script swapping the duck animation for a custom
// pose (prone, crawl, ...) needs to set the matching collision scale itself - the engine
// can't infer one from an arbitrary animation the way the native 0.5x duck shrink infers
// its own. Checked ahead of bIsDucking everywhere below, so an explicit scale always wins
// and the native duck shrink never fights a script that has taken over: once a script sets
// one, the ped's collision stays at that scale regardless of the native duck flag, until
// the script clears it (a negative scale) again.
//
// Stores the CLAMPED scale actually in effect, never a raw unclamped caller value - see
// SetPedCollisionHeight/GetPedCollisionHeight further down.
static std::unordered_map<CPedSAInterface*, float> ms_PedExplicitCollisionScales;

// setPedScale's (Client/multiplayer_sa/CMultiplayerSA_PedScale.cpp) collision half - a script
// asking for a whole bigger/smaller ped, not just a shorter one, scales all three axes of the
// same override this file already builds for height. Kept as a separate map rather than
// folded into ms_PedExplicitCollisionScales above: the two features are independent (one
// script API scales height only within the model's own footprint, the other scales the whole
// body including that footprint), and giving each its own map keeps that independence obvious
// instead of overloading one float's meaning based on which caller set it. Takes priority over
// ms_PedExplicitCollisionScales when both are set - see the precedence comment on
// GetPedDuckColModelOverride below; a script wanting a crouching giant needs to manage that
// itself for now, e.g. by re-issuing setPedScale with its own reduced value while crouched.
static std::unordered_map<CPedSAInterface*, float> ms_PedExplicitBodyScales;

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

// Legal range for a script's explicit collision scale (setPedCollisionHeight). Both ends are
// deliberately conservative rather than fitted to one discovered engine constant - nothing
// resembling a hardcoded "minimum col model height" turned up anywhere in CCollision or
// CPhysical (checked via gta-reversed and fresh Ghidra decompilation of the functions this
// feature hooks), so there is no single authoritative number to match.
//
// MIN keeps the override box comfortably non-degenerate - CreatePedColModelWithScale's own
// bounding-sphere formula stays well-defined at this floor, and 5% of standing height is
// still flat enough to satisfy the original "fit through a low gap" request without reaching
// for a literal zero-height box.
//
// MAX exists because of a real, separately-verified limitation: CPhysical's own per-tick
// "which nearby entities are even worth testing against me this frame" scan (e.g.
// CPhysical::ProcessShiftSectorList, 0x546670, and its siblings) reads its search radius from
// CModelInfo::GetModelInfo(modelIndex)->GetColModel()->GetBoundRadius() - the SHARED model's
// own col model, fetched directly, NOT through the entity's virtual GetColModel() that
// HOOK_CEntity_GetColModel below intercepts. A scaled-up ped's override is invisible to that
// search radius, so a large enough scale can grow this ped's real collision box past the
// distance the engine ever scans for candidate geometry, letting its extremities poke through
// world/vehicle collision it was never even tested against. 5.0x keeps that blind spot small
// relative to the normal search radius; it narrows the risk, it does not remove it - see the
// commit message.
constexpr float MIN_COLLISION_SCALE = 0.05f;
constexpr float MAX_COLLISION_SCALE = 5.0f;

// Builds a private copy of a model's shared CColModel with its height scaled to fScale times
// the model's own standing height, keeping the ped's feet planted. Shared by the native duck
// shrink and a script's explicit setPedCollisionHeight - both just pick a different fScale
// and land in the same override. fScale is expected to already be clamped to a sane range by
// the caller (SetPedCollisionHeight or setPedScale's collision half, each with its own
// constants); the check here only guards against a degenerate (non-positive) standing height
// on the source model itself.
//
// bScaleFootprint is false for every existing setPedCollisionHeight caller (height-only, the
// original design this function shipped with - X/Y stay exactly as the source model had them)
// and true only for setPedScale's collision half, which wants the whole body - X/Y footprint
// included - scaled by the same fScale. X/Y are scaled directly around local (0, 0) rather
// than recentred like Z: GTA:SA ped models are authored symmetric left-right and front-back
// around their own origin, so multiplying m_vecMin/m_vecMax's X and Y by fScale keeps the
// footprint centred for free, and it matches how the VISUAL scale (RwMatrixScale on the root
// frame, see CMultiplayerSA_PedScale.cpp) also scales around that same local origin - collision
// and visuals end up geometrically consistent with each other.
//
// m_data is copied as a raw pointer, not deep-copied: it keeps pointing at the shared
// collision mesh. Peds don't carry meaningful per-triangle collision (their push-back test
// runs off the bound box/sphere only - see CPed::ProcessEntityCollision, 0x5E2530), and
// multiple ped instances of one model already share and scratch-mutate that same data each
// frame in retail gameplay, so sharing it here doesn't add any new risk. Only the box and
// sphere below get new values.
static CColModelSAInterface* CreatePedColModelWithScale(CColModelSAInterface* pSharedColModel, float fScale, bool bScaleFootprint = false)
{
    auto* pOverride = new CColModelSAInterface(*pSharedColModel);

    const float fMinZ = pOverride->m_bounds.m_vecMin.fZ;
    const float fStandHeight = pOverride->m_bounds.m_vecMax.fZ - fMinZ;
    const float fNewHeight = (fStandHeight > 0.0f) ? fStandHeight * fScale : fStandHeight;

    pOverride->m_bounds.m_vecMax.fZ = fMinZ + fNewHeight;

    if (bScaleFootprint)
    {
        pOverride->m_bounds.m_vecMin.fX *= fScale;
        pOverride->m_bounds.m_vecMax.fX *= fScale;
        pOverride->m_bounds.m_vecMin.fY *= fScale;
        pOverride->m_bounds.m_vecMax.fY *= fScale;
    }

    // Recompute the bounding sphere from scratch instead of scaling the original in place.
    // CCollision::ProcessColModels (0x4185C0, verified against gta-reversed's reversed source)
    // gates the ENTIRE collision test between two entities on a sphere-vs-box check first:
    // this ped's own bounding sphere, transformed into the other shape's space, tested against
    // the other shape's bounding box - if that fails, zero collision points come back and
    // nothing further is even tested, no matter what the box or mesh would otherwise have
    // said. That makes the sphere load-bearing, not decorative, so it has to be a real
    // bounding sphere of the NEW box on every call, not a linear rescale of whatever the
    // original model's sphere happened to look like (which, per the previous version of this
    // comment, isn't even guaranteed to be centred on the box's Z midpoint to begin with).
    // Centring it on the box and sizing it to the box's half-diagonal guarantees it always
    // fully encloses the box, for a shrink or a grow, regardless of the source model.
    const CVector vecHalfExtent = (pOverride->m_bounds.m_vecMax - pOverride->m_bounds.m_vecMin) * 0.5f;
    pOverride->m_sphere.m_center = pOverride->m_bounds.m_vecMin + vecHalfExtent;
    pOverride->m_sphere.m_radius = vecHalfExtent.Length();

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

// Defined in CMultiplayerSA_PedScale.cpp - the same ped-destructor hook below that clears
// this file's own per-ped state also has to clear that file's visual-scale entry, for the
// same reason: leaving it would let a future, unrelated ped at the same freed address inherit
// a stale scale. Declared extern here rather than shared through a header, matching how this
// hook file already only exposes its own surface through CMultiplayerSA.h.
extern void ReleasePedVisualScale(CPedSAInterface* pPed);

// Full cleanup for a ped that's going away: the cached override, any explicit collision-height
// scale (setPedCollisionHeight), any explicit body scale (setPedScale's collision half), and
// setPedScale's visual half. Distinct from ReleasePedDuckColModel above, which callers that
// are only reacting to a state change (native duck ending, a new explicit scale replacing the
// cached shape) need to keep separate from the explicit scales themselves -
// ReleasePedDuckColModel runs while one is still very much in effect, just about to be
// rebuilt against it.
static void ReleaseAllPedCollisionState(CPedSAInterface* pPed)
{
    ReleasePedDuckColModel(pPed);
    ms_PedExplicitCollisionScales.erase(pPed);
    ms_PedExplicitBodyScales.erase(pPed);
    ReleasePedVisualScale(pPed);
}

// The single place that owns the override's lifetime, tied to the native bIsDucking flag and
// the two explicit-scale maps above: called from both the GetColModel hook (this ped as the
// *other* party in someone else's collision test) and the ProcessEntityCollision hook (this
// ped's *own* shape when it's the one being tested against the world) - both need the same
// answer. Precedence, highest first: an explicit body scale (setPedScale) beats an explicit
// height-only scale (setPedCollisionHeight), which beats the native bIsDucking shrink. A body
// scale wins outright rather than composing with a height scale - see the comment on
// ms_PedExplicitBodyScales above for why, and what a script wanting both needs to do itself.
static CColModelSAInterface* GetPedDuckColModelOverride(CPedSAInterface* pPed)
{
    auto       bodyIter = ms_PedExplicitBodyScales.find(pPed);
    const bool bHasBodyScale = bodyIter != ms_PedExplicitBodyScales.end();

    auto       explicitIter = ms_PedExplicitCollisionScales.find(pPed);
    const bool bHasExplicitScale = explicitIter != ms_PedExplicitCollisionScales.end();

    if (!bHasBodyScale && !bHasExplicitScale && !pPed->pedFlags.bIsDucking)
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

    CColModelSAInterface* pOverride;
    if (bHasBodyScale)
    {
        pOverride = CreatePedColModelWithScale(pSharedColModel, bodyIter->second, true);
    }
    else
    {
        // DUCK_HEIGHT_SCALE is already a scale (see its own comment), so both paths hand
        // CreatePedColModelWithScale the same kind of value now - no separate absolute-height
        // computation needed here any more.
        const float fScale = bHasExplicitScale ? explicitIter->second : DUCK_HEIGHT_SCALE;
        pOverride = CreatePedColModelWithScale(pSharedColModel, fScale);
    }

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

    // An explicit script scale - either setPedCollisionHeight's height-only one or setPedScale's
    // whole-body one - wins over bIsDucking (see ms_PedExplicitCollisionScales and
    // ms_PedExplicitBodyScales above), so clearing bIsDucking here wouldn't actually change
    // this ped's collision shape - the native duck task ending has nothing to do with it any
    // more, and nothing to block for. Still correct under either scale-based feature: a script
    // that has taken explicit control of this ped's collision (any scale, however small or
    // large) still owns the "can it stand up" question itself.
    if (ms_PedExplicitCollisionScales.find(pPed) != ms_PedExplicitCollisionScales.end())
        return false;

    if (ms_PedExplicitBodyScales.find(pPed) != ms_PedExplicitBodyScales.end())
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
// Releases a ped's override and any explicit height, if it has either, before its interface
// memory can be freed and reused by a future, unrelated ped at the same address - without
// this, that future ped could inherit a stale override (or a script's height meant for a
// completely different ped).
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
        call    ReleaseAllPedCollisionState
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
// CMultiplayerSA::SetPedCollisionHeight / GetPedCollisionHeight
//
// Lua-facing entry points (setPedCollisionHeight/getPedCollisionHeight) for the explicit
// scale map above. fScale is a multiplier of THIS PED'S OWN normal standing height - 1.0 is
// unchanged, 0.5 is half height, 2.0 is double - not an absolute height in raw engine units
// the way the first version of this API worked. That original design gave a caller no
// intuitive reference point (what does passing "3" even mean - 3 what, relative to what?),
// and let an arbitrary absolute value produce a wildly disproportionate box/sphere pair
// relative to the model's real size - scale is always relative to this ped's own standing
// height, so 1.0 means "normal" no matter which ped model it's applied to.
//
// A negative fScale clears the override, going back to the native duck shrink (if the ped
// happens to be ducking) or the model's normal shared collision. 0 and any positive value is
// a real, legal scale request, clamped to [MIN_COLLISION_SCALE, MAX_COLLISION_SCALE]. A
// negative value was picked as the clear/reset sentinel, rather than adding a separate
// resetPedCollisionHeight function, because a negative-height collision box is already
// physically meaningless - reclaiming it costs the legal range nothing. That's the opposite
// of the previous height<=0 sentinel, which swallowed 0 and small positives a script might
// legitimately want for an extremely flat "pancake" collision shape.
//
// Everything else - the GetColModel/ProcessEntityCollision redirection, the per-ped CColModel
// cache, the destructor cleanup - is the same plumbing the native duck shrink already uses;
// this only ever touches ms_PedExplicitCollisionScales plus the one cache entry that needs
// invalidating when it changes.
//
//////////////////////////////////////////////////////////////////////////////////////////
bool CMultiplayerSA::SetPedCollisionHeight(CPlayerPed* pPed, float fScale)
{
    if (!pPed)
        return false;

    auto* pPedInterface = pPed->GetPedInterface();
    if (!pPedInterface)
        return false;

    if (fScale < 0.0f)
        ms_PedExplicitCollisionScales.erase(pPedInterface);
    else
        ms_PedExplicitCollisionScales[pPedInterface] = std::clamp(fScale, MIN_COLLISION_SCALE, MAX_COLLISION_SCALE);

    // Drop any cached override built for the old state (old explicit scale, or the native
    // duck shrink an explicit scale might now be overriding) - the next collision query
    // rebuilds it, at the new scale, from the precedence in GetPedDuckColModelOverride.
    ReleasePedDuckColModel(pPedInterface);
    return true;
}

bool CMultiplayerSA::GetPedCollisionHeight(CPlayerPed* pPed, float& fScale)
{
    if (!pPed)
        return false;

    auto* pPedInterface = pPed->GetPedInterface();
    if (!pPedInterface)
        return false;

    auto iter = ms_PedExplicitCollisionScales.find(pPedInterface);
    if (iter == ms_PedExplicitCollisionScales.end())
        return false;

    // The clamped scale actually applied, which may differ from whatever raw value was
    // originally passed to SetPedCollisionHeight if that was out of range.
    fScale = iter->second;
    return true;
}

//////////////////////////////////////////////////////////////////////////////////////////
//
// CMultiplayerSA::SetPedCollisionBodyScale
//
// The collision half of setPedScale - called by CMultiplayerSA::SetPedScale
// (CMultiplayerSA_PedScale.cpp, a sibling method of this same class, not a separate SDK entry
// point), which also drives the visual half. Kept as its own method rather than folded into
// SetPedCollisionHeight above: SetPedScale already knows its own legal range
// (MIN/MAX_PED_SCALE in CMultiplayerSA_PedScale.cpp) and clamps to it before calling here, so
// fScale arrives pre-clamped; this only re-clamps to this file's own MIN/MAX_COLLISION_SCALE
// as a cheap defensive backstop in case that ever changes independently, not because the two
// ranges are expected to differ in practice.
//
// A negative fScale clears the override the same way SetPedCollisionHeight's does.
//
//////////////////////////////////////////////////////////////////////////////////////////
bool CMultiplayerSA::SetPedCollisionBodyScale(CPlayerPed* pPed, float fScale)
{
    if (!pPed)
        return false;

    auto* pPedInterface = pPed->GetPedInterface();
    if (!pPedInterface)
        return false;

    if (fScale < 0.0f)
        ms_PedExplicitBodyScales.erase(pPedInterface);
    else
        ms_PedExplicitBodyScales[pPedInterface] = std::clamp(fScale, MIN_COLLISION_SCALE, MAX_COLLISION_SCALE);

    ReleasePedDuckColModel(pPedInterface);
    return true;
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
