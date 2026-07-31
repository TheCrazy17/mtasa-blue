/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto v1.0
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        game_sa/CRopesSA.cpp
 *  PURPOSE:     Rope entity
 *
 *  Multi Theft Auto is available from https://www.multitheftauto.com/
 *
 *****************************************************************************/

#include "StdInc.h"
#include "CRopesSA.h"
#include "CRopeInstanceSA.h"
#include "CGameSA.h"

extern CGameSA* pGame;

DWORD dwDurationAddress = 0x558D1El;

CRopesSAInterface (&CRopesSA::ms_aRopes)[8] = *(CRopesSAInterface (*)[8])0xB768B8;

int CRopesSA::CreateRopeForSwatPed(const CVector& vecPosition, DWORD dwDuration)
{
    int      iReturn;
    DWORD    dwFunc = FUNC_CRopes_CreateRopeForSwatPed;
    CVector* pvecPosition = const_cast<CVector*>(&vecPosition);
    // First Push @ 0x558D1D is the duration.
    MemPut((void*)(dwDurationAddress), dwDuration);
    // clang-format off
    __asm
    {
        push    pvecPosition
        call    dwFunc
        add     esp, 0x4
        mov     iReturn, eax
    }
    // clang-format on
    //   Set it back for SA in case we ever do some other implementation.
    MemPut((DWORD*)(dwDurationAddress), 4000);
    return iReturn;
}

void CRopesSA::RemoveEntityRope(CEntitySAInterface* pEntity)
{
    CRopesSAInterface* pRope = nullptr;

    for (uint i = 0; i < ROPES_COUNT; i++)
    {
        if (ms_aRopes[i].m_pRopeEntity == pEntity)
        {
            pRope = &ms_aRopes[i];
            break;
        }
    }

    if (pRope)
    {
        auto CRope_Remove = (void(__thiscall*)(CRopesSAInterface*))0x556780;
        CRope_Remove(pRope);
    }
}

CRopeInstanceSA* CRopesSA::FindPooledRope(int ropeHandle)
{
    auto it = m_pooledRopes.find(ropeHandle);
    return it != m_pooledRopes.end() ? it->second.get() : nullptr;
}

int CRopesSA::CreateRope(int ropeType, const CVector& vecPosition, CEntitySAInterface* pHolder)
{
    auto pRope = std::make_unique<CRopeInstanceSA>();
    if (!pRope->Create(static_cast<eRopeTypeSA>(ropeType), vecPosition, pHolder))
        return -1;

    int iHandle = m_nextRopeHandle++;
    m_pooledRopes[iHandle] = std::move(pRope);
    return iHandle;
}

void CRopesSA::DestroyRope(int ropeHandle)
{
    if (auto* pRope = FindPooledRope(ropeHandle))
    {
        pRope->Destroy();
        m_pooledRopes.erase(ropeHandle);
    }
}

void CRopesSA::UpdateRopes()
{
    for (auto& [handle, pRope] : m_pooledRopes)
        pRope->Update();
}

void CRopesSA::RenderRopes()
{
    for (auto& [handle, pRope] : m_pooledRopes)
        pRope->Render();
}

namespace
{
    // Renders every pooled (relocated, outside the native 8-slot) rope from the exact same call site
    // the game renders its own native ropes from (RenderEffects(), 0x53E170 -> CRopes::Render(), 0x556AE0),
    // instead of from the logic-tick Pulse(). RenderWare's Im3D pipeline used by CRope::Render() reads
    // per-frame state (current camera raster etc.) that RW only sets up during its actual render pass -
    // calling it from the logic tick null-derefs one of those globals and crashes.
    void __cdecl RenderPooledRopes()
    {
        pGame->GetRopes()->RenderRopes();
    }

// CRopes::Render() (0x556AE0) ends with `pop esi; ret` at 0x556B05/0x556B06, followed by 9 bytes of
// NOP padding before the next function (CRopes::Shutdown, 0x556B10) - room enough for a 5-byte jmp
// without touching either function's real code.
#define HOOKPOS_CRopes_Render  0x556B05
#define HOOKSIZE_CRopes_Render 5
    void __declspec(naked) HOOK_CRopes_Render()
    {
        MTA_VERIFY_HOOK_LOCAL_SIZE;
        // clang-format off
        __asm
        {
            pop     esi
            call    RenderPooledRopes
            ret
        }
        // clang-format on
    }

    // CRope::Update() (0x557530) has a "fell into the void" safety net: if the rope's first segment
    // drops below world Z -50, it calls CRope::Remove() on itself outright - deletes the hook/magnet
    // prop and marks the slot free. Vanilla ropes never trip this because they're always anchored high
    // up on a real crane; a scripted rope with no real holder can fall through the ground (the rope's
    // own segment simulation doesn't collide with world geometry - see CRopeInstanceSA::GetHookPosition)
    // and hit it, silently deleting the rope with no way for a script to know why.
    //
    // This same code runs for the native ms_aRopes[8] slots too (e.g. a SWAT rappel rope), where the
    // cleanup is legitimate and must stay - so this hook only skips the Remove() call when `this` (esi
    // at the call site) is one of OUR pooled ropes, i.e. NOT inside the native ms_aRopes[8] array
    // (0xB768B8 .. 0xB768B8 + 8*sizeof(SRopeInstanceSA)). Native ropes fall through to the original
    // `call 0x556780` untouched.
    //
    // The hook overwrites `mov ecx, esi; call 0x556780` (7 bytes at 0x558CC0) with a 5-byte jmp; the
    // trailing 2 bytes of the old `call` become unreachable, which is fine since nothing jumps into the
    // middle of it. Both continuation targets (0x558CC7 for native ropes, 0x558CCE for ours) are chosen
    // to keep behaviour identical to vanilla in every other respect: native ropes still clear the flag
    // byte at [esi+0x327] the original code cleared after Remove(); pooled ropes skip straight past that
    // too, since it isn't ours to touch and nothing else needs it to be cleared for them to keep working.
#define HOOKPOS_CRope_Update_VoidRemove  0x558CC0
#define HOOKSIZE_CRope_Update_VoidRemove 5
    static constexpr intptr_t RETURN_CRope_Update_VoidRemove_Native = 0x558CC7;
    static constexpr intptr_t RETURN_CRope_Update_VoidRemove_Pooled = 0x558CCE;
    void __declspec(naked) HOOK_CRope_Update_VoidRemove()
    {
        MTA_VERIFY_HOOK_LOCAL_SIZE;
        // clang-format off
        __asm
        {
            mov     ecx, esi
            cmp     esi, 0xB768B8
            jb      pooled_rope
            cmp     esi, 0xB78248            ; 0xB768B8 + 8 * 0x328 (sizeof(SRopeInstanceSA)), end of ms_aRopes
            jae     pooled_rope
            mov     eax, 0x556780            ; native rope - preserve vanilla "fell into the void" cleanup
            call    eax
            jmp     RETURN_CRope_Update_VoidRemove_Native
        pooled_rope:
            jmp     RETURN_CRope_Update_VoidRemove_Pooled
        }
        // clang-format on
    }
}            // namespace

void CRopesSA::StaticSetHooks()
{
    HookInstall(HOOKPOS_CRopes_Render, (DWORD)HOOK_CRopes_Render, HOOKSIZE_CRopes_Render);
    HookInstall(HOOKPOS_CRope_Update_VoidRemove, (DWORD)HOOK_CRope_Update_VoidRemove, HOOKSIZE_CRope_Update_VoidRemove);
}

bool CRopesSA::AttachRopeToEntity(int ropeHandle, CEntitySAInterface* pEntity)
{
    auto* pRope = FindPooledRope(ropeHandle);
    return pRope ? pRope->PickUpObject(pEntity) : false;
}

void CRopesSA::DetachRopeEntity(int ropeHandle)
{
    if (auto* pRope = FindPooledRope(ropeHandle))
        pRope->ReleasePickedUpObject();
}

CEntitySAInterface* CRopesSA::GetRopeAttachedEntity(int ropeHandle)
{
    auto* pRope = FindPooledRope(ropeHandle);
    return pRope ? pRope->GetAttachedEntity() : nullptr;
}

bool CRopesSA::IsEntityAttachedToRope(CEntitySAInterface* pEntity)
{
    if (!pEntity)
        return false;

    for (auto& [handle, pRope] : m_pooledRopes)
    {
        if (pRope->GetAttachedEntity() == pEntity)
            return true;
    }
    return false;
}

int CRopesSA::GetRopeType(int ropeHandle)
{
    auto* pRope = FindPooledRope(ropeHandle);
    return pRope ? static_cast<int>(pRope->GetRopeType()) : -1;
}

bool CRopesSA::SetRopeSegmentLength(int ropeHandle, float length)
{
    auto* pRope = FindPooledRope(ropeHandle);
    if (!pRope)
        return false;
    pRope->SetSegmentLength(length);
    return true;
}

float CRopesSA::GetRopeSegmentLength(int ropeHandle)
{
    auto* pRope = FindPooledRope(ropeHandle);
    return pRope ? pRope->GetSegmentLength() : 0.0f;
}

bool CRopesSA::SetRopeAnchorVelocity(int ropeHandle, const CVector& speed)
{
    auto* pRope = FindPooledRope(ropeHandle);
    if (!pRope)
        return false;
    pRope->SetAnchorVelocity(speed);
    return true;
}

bool CRopesSA::GetRopeHookPosition(int ropeHandle, CVector& outPosition)
{
    auto* pRope = FindPooledRope(ropeHandle);
    return pRope ? pRope->GetHookPosition(outPosition) : false;
}

bool CRopesSA::GetRopeSegmentPosition(int ropeHandle, uint8 index, CVector& outPosition)
{
    if (index >= ROPE_SEGMENT_COUNT)
        return false;

    auto* pRope = FindPooledRope(ropeHandle);
    if (!pRope)
        return false;

    outPosition = pRope->GetSegmentPosition(index);
    return true;
}
