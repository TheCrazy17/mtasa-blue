/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        multiplayer_sa/CMultiplayerSA_PedScale.cpp
 *
 *  Multi Theft Auto is available from https://www.multitheftauto.com/
 *
 *****************************************************************************/
#include "StdInc.h"
#include "..\game_sa\gamesa_renderware.h"
#include <algorithm>
#include <unordered_map>

//////////////////////////////////////////////////////////////////////////////////////////
//
// setPedScale: a full, uniform ped scale - visual mesh/skeleton together with collision
// (the collision half reuses setPedCollisionHeight's per-instance CColModel override
// mechanism, scaled uniformly across X/Y/Z instead of height-only - see
// CMultiplayerSA::SetPedCollisionBodyScale in CMultiplayerSA_PedCrouchCollision.cpp). This
// file only owns the VISUAL half: scaling the ped's root RwFrame matrix so the whole skinned
// mesh and skeleton scale together through normal RenderWare hierarchy propagation.
//
// This isn't a new mechanism invented for peds - CObject already has a working per-instance
// visual scale (setObjectScale), and its implementation is entity-level, not object-specific:
// CEntitySA::SetScaleInternal (CEntitySA.cpp) pre-multiplies a scale onto the clump's root
// frame's modelling matrix via RwMatrixScale(..., TRANSFORM_BEFORE), then RwFrameUpdateObjects
// propagates that down through every descendant frame's local-to-world matrix - ordinary RW
// hierarchy math, the same math a skinned mesh's bone matrices are derived from, so a root-
// level scale reaches the skin exactly like it reaches a simple rigid clump's child frames.
// CPed uses the same RpClump/RwFrame representation as CObject (RpAnimBlendClump IS an
// RpClump), and CEntitySAInterface::UpdateRW/UpdateRpHAnim - the two calls that wrap the
// matrix scale - are already entity-level methods, not object-specific ones (see
// CEntitySA.h/.cpp); CLuaPedDefs::UpdateElementRpHAnim already calls UpdateRpHAnim on a ped's
// CEntity directly for bone-matrix editing, so this plumbing is already proven reachable and
// working for peds specifically, not just inferred from the object case.
//
// The one thing peds need that objects mostly don't: CObject's own scale re-applies itself
// every frame rather than once (see PostCWorld_ProcessPedsAfterPreRender in CMultiplayerSA.cpp
// - it re-checks bUpdateScale/fScale on every object that required a pre-render this frame,
// not just once when SetScale was called), which is itself evidence that something in the
// native per-frame pipeline doesn't reliably preserve a custom root scale across frames. Peds
// recompute their whole skeleton from animation data every single frame - far more aggressively
// than a mostly-static object's frame hierarchy - so this file follows the exact same
// every-frame-reapplication pattern via the exact same hook point, extended to also cover
// peds (see ApplyExplicitPedScales, called from PostCWorld_ProcessPedsAfterPreRender).
//
// NOT SYNCED TO OTHER PLAYERS - deliberately, for this pass. Unlike setPedCollisionHeight
// (collision-only, so it only ever affects this client's own local physics resolution -
// nothing another client needs to know about), a VISUAL scale is meaningless unless everyone
// sees the same giant/tiny ped, which needs real netcode: this is scoped out here, not because
// it's unclear how, but to keep this pass to the part that was actually investigated and
// verified end to end. The mechanism to reuse for a follow-up is setObjectScale's, traced in
// full while researching this feature:
//   - Server: CObject::m_vecScale (Server/mods/deathmatch/logic/CObject.h/.cpp) is a plain
//     persisted field, set by CStaticFunctionDefinitions::SetObjectScale.
//   - New-relevance sync: CEntityAddPacket.cpp writes that field whenever the object is
//     (re)streamed in for a client - including a client who joins or streams in long after
//     the scale was set - inside its `case CElement::OBJECT:`/`case CElement::PED:`-style
//     switch. CElement::PED's case already writes health/armor/clothes/weapons/etc. there;
//     scale would be one more field alongside them.
//   - Live update: CStaticFunctionDefinitions::SetObjectScale also broadcasts a small
//     SET_OBJECT_SCALE CElementRPCPacket (CObjectRPCs.cpp defines the client-side handler) to
//     already-joined players, for immediate effect without waiting for a re-stream.
// A ped equivalent needs: a persisted server-side CPed scale field, one more write in
// CEntityAddPacket's `case CElement::PED:` block, a SET_PED_SCALE RPC (client handler
// mirroring CObjectRPCs::SetObjectScale, calling into CClientPed::SetScale), and the
// corresponding server-side Lua bindings. None of it is speculative - it's the same pattern
// setObjectScale already ships - it's simply more files than this pass covers.
//
// KNOWN GAPS - identified, not fixed here (scope explicitly allows this - see the setPedScale
// investigation notes for the full reasoning):
//   - CPhysical's own per-tick "which nearby entities are worth testing against me" scan
//     reads its search radius from the SHARED model's own bound radius
//     (CModelInfo::GetModelInfo(idx)->GetColModel()->GetBoundRadius()), not through the
//     CEntity::GetColModel virtual call setPedCollisionHeight's override hooks - the exact
//     same blind spot documented on MAX_COLLISION_SCALE in CMultiplayerSA_PedCrouchCollision.cpp,
//     equally applicable to a large setPedScale.
//   - Feet-anchoring is unverified: RwMatrixScale scales around the root frame's own local
//     origin, and this file does not compensate the entity's world position afterward. If
//     that local origin isn't already ground-level (collision bounds put it roughly 1 unit
//     above the feet - see CMultiplayerSA_PedCrouchCollision.cpp), a grown/shrunk ped could
//     visually float or sink slightly relative to where it's actually standing. Deliberately
//     NOT auto-corrected: the correct compensation depends on knowing which case is true, and
//     guessing wrong would make a real problem out of what might not even be one. Needs an
//     in-game check before relying on this for anything where exact foot placement matters.
//   - Bone-relative attachments (held weapons, weapon FX - confirmed via CPed::GetBoneMatrix
//     callers in gta-reversed, e.g. the molotov flame effect anchoring to BONE_R_HAND) read
//     the LIVE bone matrix each frame, which inherits this root-level scale through ordinary
//     RW hierarchy propagation, so they should track a scaled ped correctly with no extra
//     work - reasoned from how bone queries work, not from having watched a scaled ped fire a
//     weapon in-game.
//   - Camera-follow distance, footstep/surface-audio trigger points, and any other hardcoded
//     "peds are about this big" assumption elsewhere in the engine were not exhaustively
//     audited - genuinely unknown, not asserted safe.
//
//////////////////////////////////////////////////////////////////////////////////////////

static std::unordered_map<CPedSAInterface*, float> ms_PedVisualScales;

// Legal range for setPedScale, applied to both the visual and collision halves. Same
// reasoning and same values as setPedCollisionHeight's MIN/MAX_COLLISION_SCALE
// (CMultiplayerSA_PedCrouchCollision.cpp) - a separate pair of constants because the two
// features are independent (see the precedence comment on ms_PedExplicitBodyScales in that
// file), not because a different range was found to be more appropriate here. In particular,
// the MAX_COLLISION_SCALE comment's caveat about CPhysical's per-tick nearby-entity search
// radius reading the unscaled model radius applies identically to a large setPedScale.
constexpr float MIN_PED_SCALE = 0.05f;
constexpr float MAX_PED_SCALE = 5.0f;

// Scales this ped's clump root frame in place. Mirrors CEntitySA::SetScaleInternal +
// CEntitySA::UpdateRpHAnim (CEntitySA.cpp) exactly, just operating on the raw interface the
// way the rest of multiplayer_sa's hook-level code already does here, rather than going
// through a CPed/CEntitySA wrapper object.
static void ApplyPedVisualScale(CPedSAInterface* pPed, float fScale)
{
    pPed->UpdateRW();

    RpClump* clump = pPed->m_pRwObject;
    if (!clump)
        return;

    auto*         frame = reinterpret_cast<RwFrame*>(clump->object.parent);
    const CVector vecScale(fScale, fScale, fScale);
    RwMatrixScale(reinterpret_cast<RwMatrix*>(&frame->modelling), reinterpret_cast<const RwV3d*>(&vecScale), TRANSFORM_BEFORE);
    RwFrameUpdateObjects(frame);

    // Re-runs the anim-blend hierarchy update, matching what the object loop in
    // PostCWorld_ProcessPedsAfterPreRender does for any clump-type object. Peds are always
    // clump-type, so this always runs for them (unlike the conditional check there).
    pPed->UpdateRpHAnim();
}

// Called once per frame from PostCWorld_ProcessPedsAfterPreRender (CMultiplayerSA.cpp), right
// alongside that function's own object-scale loop and for the same reason - see this file's
// header comment. Cheap early-out when nothing is scaled, which is the common case.
void CMultiplayerSA::ApplyExplicitPedScales()
{
    if (ms_PedVisualScales.empty())
        return;

    for (const auto& [pPedInterface, fScale] : ms_PedVisualScales)
        ApplyPedVisualScale(pPedInterface, fScale);
}

// Declared extern and called from CMultiplayerSA_PedCrouchCollision.cpp's ped-destructor
// hook, which already has to clear that file's own per-ped state for the same reason: a
// future, unrelated ped reusing this freed interface address must not inherit a stale scale.
void ReleasePedVisualScale(CPedSAInterface* pPed)
{
    ms_PedVisualScales.erase(pPed);
}

//////////////////////////////////////////////////////////////////////////////////////////
//
// CMultiplayerSA::SetPedScale / GetPedScale
//
// Lua-facing entry points (setPedScale/getPedScale). Drives the visual half (this file) and
// the collision half (SetPedCollisionBodyScale, CMultiplayerSA_PedCrouchCollision.cpp)
// together from one call, clamped once here to [MIN_PED_SCALE, MAX_PED_SCALE] and handed to
// both halves pre-clamped. A negative fScale clears both halves, same sentinel convention as
// setPedCollisionHeight and for the same reason (a negative scale is already physically
// meaningless, so reclaiming it as "reset" costs the legal range nothing).
//
// Deliberately uniform-only (one float, not a per-axis CVector like setObjectScale takes):
// non-uniform scale on a skinned, bone-animated rig is a materially different and unverified
// risk (skin weights and bone matrices interacting with independently-scaled axes) from the
// simple, verified case of scaling a whole clump hierarchy by the same factor on every axis.
// Restricting to uniform keeps this feature inside the confidence this file's header comment
// argues for; non-uniform ped scale, if ever wanted, is a separate investigation.
//
//////////////////////////////////////////////////////////////////////////////////////////
bool CMultiplayerSA::SetPedScale(CPlayerPed* pPed, float fScale)
{
    if (!pPed)
        return false;

    auto* pPedInterface = pPed->GetPedInterface();
    if (!pPedInterface)
        return false;

    if (fScale < 0.0f)
        ms_PedVisualScales.erase(pPedInterface);
    else
        ms_PedVisualScales[pPedInterface] = std::clamp(fScale, MIN_PED_SCALE, MAX_PED_SCALE);

    // Same clamped value (or the same negative-clears signal) into the collision half, so the
    // two halves can never disagree about what "the current scale" is.
    SetPedCollisionBodyScale(pPed, fScale);

    return true;
}

bool CMultiplayerSA::GetPedScale(CPlayerPed* pPed, float& fScale)
{
    if (!pPed)
        return false;

    auto* pPedInterface = pPed->GetPedInterface();
    if (!pPedInterface)
        return false;

    auto iter = ms_PedVisualScales.find(pPedInterface);
    if (iter == ms_PedVisualScales.end())
        return false;

    fScale = iter->second;
    return true;
}
