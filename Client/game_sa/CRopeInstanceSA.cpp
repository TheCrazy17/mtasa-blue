/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto v1.0
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        game_sa/CRopeInstanceSA.cpp
 *  PURPOSE:     Standalone (non-pooled) SA rope instance
 *
 *  Multi Theft Auto is available from https://www.multitheftauto.com/
 *
 *****************************************************************************/

#include "StdInc.h"
#include <core/CCoreInterface.h>
#include <game/CModelInfo.h>
#include "CRopeInstanceSA.h"
#include "CGameSA.h"
#include "CRopesSA.h"
#include "CEntitySA.h"

extern CCoreInterface* g_pCore;
extern CGameSA*        pGame;

#define ROPES_ARRAY_ADDRESS 0xB768B8
#define ROPES_ARRAY_COUNT   8

#define FUNC_CRope_Remove                  0x556780
#define FUNC_CRope_Update                  0x557530
#define FUNC_CRope_Render                  0x556800
#define FUNC_CRope_PickUpObject            0x5569C0
#define FUNC_CRope_ReleasePickedUpObj      0x556030
#define FUNC_CRope_CreateHookObjectForRope 0x556070

namespace
{
    SRopeInstanceSA(&NativeRopeSlots())[ROPES_ARRAY_COUNT]
    {
        return *reinterpret_cast<SRopeInstanceSA(*)[ROPES_ARRAY_COUNT]>(ROPES_ARRAY_ADDRESS);
    }

    // Temporary diagnostic: prints exactly where inside the native code the fault happened, so the
    // crash site can be found in a disassembler instead of bisecting the whole (huge) native function.
    LONG WINAPI DumpRopeException(const char* szWhere, _EXCEPTION_POINTERS* pExceptionInfo)
    {
        EXCEPTION_RECORD* pRecord = pExceptionInfo->ExceptionRecord;
        DWORD              dwAccessType = pRecord->NumberParameters > 0 ? static_cast<DWORD>(pRecord->ExceptionInformation[0]) : 0xFFFFFFFF;
        DWORD              dwAccessAddress = pRecord->NumberParameters > 1 ? static_cast<DWORD>(pRecord->ExceptionInformation[1]) : 0;

        g_pCore->GetConsole()->Printf("[Rope] %s: code=0x%08X eip=0x%08X %s addr=0x%08X", szWhere, pRecord->ExceptionCode,
                                       reinterpret_cast<DWORD>(pRecord->ExceptionAddress), dwAccessType == 1 ? "writing" : "reading", dwAccessAddress);
        return EXCEPTION_EXECUTE_HANDLER;
    }

    float BitsToFloat(uint32 bits)
    {
        float f;
        memcpy(&f, &bits, sizeof(f));
        return f;
    }

    // CRopes::RegisterRope() (0x556B40) sets m_fMass/m_fTotalLength from a per-type constant table
    // (0x557514) before ever touching CreateHookObjectForRope. We only ever call RegisterRope
    // (indirectly, via CreateRopeForSwatPed) with type SWAT, so our borrowed slot always carries
    // SWAT's constants (mass=20, totalLength=0.645...) - values decoded straight from that jump table
    // for the type actually requested, so retargeting the type also carries over the physically
    // correct mass/reach instead of silently keeping SWAT's.
    void GetNativeMassAndTotalLength(eRopeTypeSA ropeType, float& fMass, float& fTotalLength)
    {
        switch (ropeType)
        {
            case eRopeTypeSA::MAGNET:
                fMass = BitsToFloat(0x41200000);
                fTotalLength = BitsToFloat(0x3ea5294a);
                return;
            case eRopeTypeSA::CRANE_MAGNO:
                fMass = BitsToFloat(0x42480000);
                fTotalLength = BitsToFloat(0x3fce739d);
                return;
            case eRopeTypeSA::WRECKING_BALL:
            case eRopeTypeSA::QUARRY_CRANE_ARM:
            case eRopeTypeSA::CRANE_TROLLEY:
                fMass = BitsToFloat(0x42880000);
                fTotalLength = BitsToFloat(0x400c6319);
                return;
            case eRopeTypeSA::CRANE_MAGNET1:
            case eRopeTypeSA::CRANE_HARNESS:
            case eRopeTypeSA::SWAT:
            default:
                fMass = BitsToFloat(0x41a00000);
                fTotalLength = BitsToFloat(0x3f25294a);
                return;
        }
    }

    // CRope::CreateHookObjectForRope() (0x556070) picks the hook's model from one of these game
    // globals - each resolved once at startup, by object name, from the level's object list (see
    // ModelIndices.cpp/Rope.cpp in gta-reversed) - then just does `new CObject(modelIndex, true)` with
    // NO check that the model is actually streamed in yet. The big crane magnet/harness/wrecking-ball
    // models happen to be in near-constant use across the map, so in practice they're already loaded
    // almost everywhere - MI_MINI_MAGNET ("mini_magnet") is a much more obscure model with no such
    // guarantee. Creating a CObject against an unstreamed model produces a broken entity (no collision/
    // bounding box, positions that read back as garbage - e.g. 0,0,0), which a rope with nothing else
    // anchoring it then drags around every frame, exactly matching "el magnet ... se transportada a
    // 0,0,0 ... cambiando de lugares". So: resolve which model the requested type needs and force it to
    // finish streaming in (blocking) before ever calling CreateHookObjectForRope.
#define MODELIDX_CRANE_HARNESS 0x8CD6FC
#define MODELIDX_CRANE_MAGNET  0x8CD700
#define MODELIDX_WRECKING_BALL 0x8CD71C
#define MODELIDX_MINI_MAGNET   0x8CD740

    void EnsureHookModelLoaded(eRopeTypeSA ropeType)
    {
        DWORD dwModelIndexAddress;
        switch (ropeType)
        {
            case eRopeTypeSA::CRANE_MAGNET1:
            case eRopeTypeSA::CRANE_MAGNO:
            case eRopeTypeSA::QUARRY_CRANE_ARM:
            case eRopeTypeSA::CRANE_TROLLEY:
                dwModelIndexAddress = MODELIDX_CRANE_MAGNET;
                break;
            case eRopeTypeSA::CRANE_HARNESS:
                dwModelIndexAddress = MODELIDX_CRANE_HARNESS;
                break;
            case eRopeTypeSA::MAGNET:
                dwModelIndexAddress = MODELIDX_MINI_MAGNET;
                break;
            case eRopeTypeSA::WRECKING_BALL:
                dwModelIndexAddress = MODELIDX_WRECKING_BALL;
                break;
            default:
                return;            // SWAT/NONE - CreateHookObjectForRope doesn't spawn a model for these
        }

        WORD wModelIndex = *reinterpret_cast<WORD*>(dwModelIndexAddress);
        if (wModelIndex == 0xFFFF)            // MODEL_INVALID - the name lookup never resolved this at all
            return;

        if (CModelInfo* pModelInfo = pGame->GetModelInfo(wModelIndex, true))
        {
            if (!pModelInfo->IsLoaded())
                pModelInfo->Request(EModelRequestType::BLOCKING, "Rope hook");
        }
    }
}

bool CRopeInstanceSA::Create(eRopeTypeSA ropeType, const CVector& vecPosition, CEntitySAInterface* pHolder)
{
    if (m_bValid)
        return false;

    // Snapshot which native slots are in use, so we can tell which one is ours even if real
    // gameplay ropes (SWAT rappel) already exist.
    bool bWasInUse[ROPES_ARRAY_COUNT];
    for (uint i = 0; i < ROPES_ARRAY_COUNT; i++)
        bWasInUse[i] = NativeRopeSlots()[i].m_nType != 0;

    // Borrow the already-proven native call to get a fully initialized rope, instead of guessing
    // the raw calling convention of RegisterRope ourselves.
    int iRopeId = pGame->GetRopes()->CreateRopeForSwatPed(vecPosition, 4000);
    if (iRopeId == -1)
        return false;

    int iSlot = -1;
    for (uint i = 0; i < ROPES_ARRAY_COUNT; i++)
    {
        if (!bWasInUse[i] && NativeRopeSlots()[i].m_nType != 0)
        {
            iSlot = static_cast<int>(i);
            break;
        }
    }

    if (iSlot == -1)
        return false;

    SRopeInstanceSA& nativeSlot = NativeRopeSlots()[iSlot];

    // Copy the fully-initialized rope out to our own memory...
    m_instance = nativeSlot;

    // ...then free the native slot immediately. From here on the only copy of this rope's state
    // is ours, sitting outside ms_aRopes entirely.
    auto CRope_Remove = reinterpret_cast<void(__thiscall*)(SRopeInstanceSA*)>(FUNC_CRope_Remove);
    CRope_Remove(&nativeSlot);

    // Retarget our copy to what was actually requested.
    m_instance.m_nType = static_cast<uint8>(ropeType);
    m_instance.m_pAttachedEntity = nullptr;
    m_instance.m_pRopeAttachObject = nullptr;
    GetNativeMassAndTotalLength(ropeType, m_instance.m_fMass, m_instance.m_fTotalLength);

    // CreateRopeForSwatPed() below always registers the borrowed native slot with bExpires=true and a
    // 4000ms lifetime (RegisterRope writes m_nTime = native-current-time + 4000, an absolute expiry
    // timestamp CRope::Update() checks every frame and self-Removes past). That's correct for a SWAT
    // rappel rope, not for one of ours - push the expiry timestamp out to effectively never, so our
    // copy doesn't inherit a 4-second self-destruct.
    m_instance.m_nTime = 0xFFFFFFFF;

    if (pHolder)
    {
        m_instance.m_pRopeHolder = pHolder;
    }
    else
    {
        // No holder given - pin the fake CPlaceable+CMatrix (see SFakePlaceableHolder) at the creation
        // position with an identity rotation, so the rope just stays there instead of null-deref'ing in
        // CRope::Update() (both the position-fallback path and the unconditional matrix-rotation path).
        m_fakeHolder.vecPosn = vecPosition;
        m_fakeHolder.matrix.pos = vecPosition;
        m_fakeHolder.pMatrix = &m_fakeHolder.matrix;
        m_instance.m_pRopeHolder = reinterpret_cast<CEntitySAInterface*>(&m_fakeHolder);
    }

    // RegisterRope() (invoked by CreateRopeForSwatPed above) already called CreateHookObjectForRope()
    // once, but that ran while m_nType was still SWAT - which the native function special-cases to
    // skip hook/magnet prop spawning entirely, leaving m_pAttachedEntity null. CRope::Update() (0x557530)
    // unconditionally dereferences m_pAttachedEntity for every crane/magnet/wrecking-ball type before it
    // gets around to null-checking it, so without creating the hook object for real now, with the type
    // actually requested, Update() null-derefs and crashes on the very first Pulse(). This mirrors what
    // RegisterRope would have done had it been given the real type from the start.
    EnsureHookModelLoaded(ropeType);

    auto CRope_CreateHookObjectForRope = reinterpret_cast<void(__thiscall*)(SRopeInstanceSA*)>(FUNC_CRope_CreateHookObjectForRope);

    __try
    {
        CRope_CreateHookObjectForRope(&m_instance);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        g_pCore->GetConsole()->Printf("[Rope] CRASHED inside native CreateHookObjectForRope()");
        return false;
    }

    m_bValid = true;
    return true;
}

bool CRopeInstanceSA::GetHookPosition(CVector& outPosition) const
{
    if (!m_bValid || !m_instance.m_pAttachedEntity)
        return false;

    // Same "matrix ? matrix->pos : transform.translate" fallback CEntitySA::GetPositionInternal() uses
    // (CEntitySA.cpp) - the hook is a real CObject (see CreateHookObjectForRope), so this is exactly how
    // MTA already reads any other entity's live position, just without a CEntitySA wrapper around it.
    outPosition = m_instance.m_pAttachedEntity->matrix ? m_instance.m_pAttachedEntity->matrix->vPos : m_instance.m_pAttachedEntity->m_transform.m_translate;
    return true;
}

void CRopeInstanceSA::Destroy()
{
    if (!m_bValid)
        return;

    auto CRope_Remove = reinterpret_cast<void(__thiscall*)(SRopeInstanceSA*)>(FUNC_CRope_Remove);
    CRope_Remove(&m_instance);
    m_bValid = false;
}

void CRopeInstanceSA::Update()
{
    if (!m_bValid)
        return;

    auto CRope_Update = reinterpret_cast<void(__thiscall*)(SRopeInstanceSA*)>(FUNC_CRope_Update);

    __try
    {
        CRope_Update(&m_instance);
    }
    __except (DumpRopeException("CRASHED inside native Update() on relocated memory", GetExceptionInformation()))
    {
        m_bValid = false;
        return;
    }

    // Re-assert what the script actually asked for - see the comment on SetSegmentLength().
    if (m_bHasPinnedSegmentLength)
        m_instance.m_fSegmentLength = m_fPinnedSegmentLength;
}

void CRopeInstanceSA::Render()
{
    if (!m_bValid)
        return;

    auto CRope_Render = reinterpret_cast<void(__thiscall*)(SRopeInstanceSA*)>(FUNC_CRope_Render);

    __try
    {
        CRope_Render(&m_instance);
    }
    __except (DumpRopeException("CRASHED inside native Render() on relocated memory", GetExceptionInformation()))
    {
        m_bValid = false;
    }
}

bool CRopeInstanceSA::PickUpObject(CEntitySAInterface* pEntity)
{
    if (!m_bValid || !pEntity)
        return false;

    auto CRope_PickUpObject = reinterpret_cast<void(__thiscall*)(SRopeInstanceSA*, CEntitySAInterface*)>(FUNC_CRope_PickUpObject);

    __try
    {
        CRope_PickUpObject(&m_instance, pEntity);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        g_pCore->GetConsole()->Printf("[Rope] CRASHED inside native PickUpObject() on relocated memory");
        return false;
    }
    return true;
}

void CRopeInstanceSA::ReleasePickedUpObject()
{
    if (!m_bValid)
        return;

    auto CRope_Release = reinterpret_cast<void(__thiscall*)(SRopeInstanceSA*)>(FUNC_CRope_ReleasePickedUpObj);

    __try
    {
        CRope_Release(&m_instance);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        g_pCore->GetConsole()->Printf("[Rope] CRASHED inside native ReleasePickedUpObject() on relocated memory");
    }
}
