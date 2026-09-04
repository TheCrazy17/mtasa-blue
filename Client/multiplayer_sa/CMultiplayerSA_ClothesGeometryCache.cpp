/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto v1.0
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        multiplayer_sa/CMultiplayerSA_ClothesGeometryCache.cpp
 *  PURPOSE:     Cache blended clothes geometry to reduce stutter when changing clothes
 *
 *  Multi Theft Auto is available from https://www.multitheftauto.com/
 *
 *****************************************************************************/

#include "StdInc.h"
#include "..\game_sa\gamesa_renderware.h"

// CClothesBuilder::BlendGeometry blends a clothing item's normal/fat/ripped morph targets
// together by the ped's body ratios, mutating the "normal" variant's geometry in place and
// returning it with an extra ref (the vanilla clothes builder later drops that same ref, once
// its data has been copied into the ped's merged clump geometry a bit further down the same
// rebuild; see the call site in ConstructGeometryArray and the destroy loop at the tail of
// CreateSkinnedClump, past what Ghidra's -noanalysis pass considers the end of that function).
//
// This hook caches BlendGeometry's result, keyed by its own first argument (a pointer that
// stays stable for as long as the item's model stays resident, so a stream reload of that item
// naturally shows up as a different pointer and a cache miss rather than stale data) together
// with the three blend weights. On a repeat with the same body ratios it skips the vertex/normal
// blend loop entirely.
//
// This only covers the blend math itself, not the request-and-stream-in step that happens
// earlier in ConstructGeometryArray (CClothesBuilder::RequestGeometry plus the model pump that
// follows it) -- that part still runs every time. Skipping it too would need to happen inside
// ConstructGeometryArray's own per-slot loop rather than here, and that loop also calls a
// function that fills in a per-slot bone remap table from the item's un-blended hierarchy
// (global array at 0xbbc8c8, indexed by slot) which still has to run for every real slot on
// every rebuild, cached geometry or not. Hooking BlendGeometry itself avoids that entanglement
// since it doesn't touch the caller's control flow at all, only what happens inside the call.
//
// Reference counting: RpGeometry's refs field (RenderWare.h) works the same way seen throughout
// this file's callers -- attaching one to an atomic (RpAtomicSetGeometry) takes its own ref,
// and RpGeometryDestroy (0x74ccc0) decrements and only actually frees once refs drops to its
// floor. BlendGeometry's own +1 before returning is what survives the streaming slot release
// that follows it in ConstructGeometryArray; this cache adds one more ref of its own on top of
// that before holding on to a geometry past the call that made it, and drops that same ref
// through RpGeometryDestroy on eviction or cache flush.
int _cdecl OnCClothesBuilderBlendGeometryPre(void* pItemHandle, int iNormalName, int iFatName, int iRippedName, float fWeightA, float fWeightB, float fWeightC);
void _cdecl OnCClothesBuilderBlendGeometryPost(int iResult, void* pItemHandle, int iNormalName, int iFatName, int iRippedName, float fWeightA, float fWeightB,
                                               float fWeightC);

////////////////////////////////////////////////
//
// class CClothesGeometryStore
//
// Save blended clothes geometry for later, keyed by item handle and body blend weights
//
////////////////////////////////////////////////
class CClothesGeometryStore
{
public:
    struct SCachedGeometryInfo
    {
        void*       pItemHandle;
        int         iWeightA;
        int         iWeightB;
        int         iWeightC;
        RpGeometry* pGeometry;
    };

    std::vector<SCachedGeometryInfo> cachedList;
    uint                             m_uiMaxSize = 48;

    static int Quantize(float fWeight) { return Round(fWeight * 1000.0f); }

    ///////////////////////////////////////
    //
    // Find a geometry matching the given item and blend weights. Adds our own ref on hit,
    // exactly like a freshly blended result would carry.
    //
    ///////////////////////////////////////
    RpGeometry* FindMatchAndUse(void* pItemHandle, float fWeightA, float fWeightB, float fWeightC)
    {
        int iA = Quantize(fWeightA);
        int iB = Quantize(fWeightB);
        int iC = Quantize(fWeightC);

        for (SCachedGeometryInfo& info : cachedList)
        {
            if (info.pItemHandle == pItemHandle && info.iWeightA == iA && info.iWeightB == iB && info.iWeightC == iC)
            {
                info.pGeometry->refs++;
                return info.pGeometry;
            }
        }
        return nullptr;
    }

    ///////////////////////////////////////
    //
    // Save a ref to this geometry
    //
    ///////////////////////////////////////
    void Add(void* pItemHandle, float fWeightA, float fWeightB, float fWeightC, RpGeometry* pGeometry)
    {
        if (!pGeometry)
            return;

        int iA = Quantize(fWeightA);
        int iB = Quantize(fWeightB);
        int iC = Quantize(fWeightC);

        for (const SCachedGeometryInfo& info : cachedList)
            if (info.pItemHandle == pItemHandle && info.iWeightA == iA && info.iWeightB == iB && info.iWeightC == iC)
                return;  // Already cached (shouldn't normally happen)

        if (cachedList.size() >= m_uiMaxSize)
        {
            RpGeometryDestroy(cachedList.front().pGeometry);
            cachedList.erase(cachedList.begin());
        }

        pGeometry->refs++;
        cachedList.push_back({pItemHandle, iA, iB, iC, pGeometry});
    }

    ///////////////////////////////////////
    //
    // Drop every cached geometry. Used when custom clothes textures are added or removed,
    // same trigger as CMultiplayerSA::FlushClothesCache (a model swap could also change what
    // a previously cached item handle actually points to going forward).
    //
    ///////////////////////////////////////
    void Clear()
    {
        for (const SCachedGeometryInfo& info : cachedList)
            RpGeometryDestroy(info.pGeometry);
        cachedList.clear();
    }
};

CClothesGeometryStore ms_clothesGeometryStore;

////////////////////////////////////////////////
//
// ClearClothesGeometryCache
//
// Called from CMultiplayerSA::FlushClothesCache so this cache never outlives a change to the
// underlying clothes models.
//
////////////////////////////////////////////////
void ClearClothesGeometryCache()
{
    ms_clothesGeometryStore.Clear();
}

////////////////////////////////////////////////
//
// Hook CClothesBuilder::BlendGeometry
//
//
////////////////////////////////////////////////
int _cdecl OnCClothesBuilderBlendGeometryPre(void* pItemHandle, int iNormalName, int iFatName, int iRippedName, float fWeightA, float fWeightB, float fWeightC)
{
    if (!pItemHandle)
        return 0;

    return (int)ms_clothesGeometryStore.FindMatchAndUse(pItemHandle, fWeightA, fWeightB, fWeightC);
}

////////////////////////////////////////////////////////////////////////////////////////////////
//
void _cdecl OnCClothesBuilderBlendGeometryPost(int iResult, void* pItemHandle, int iNormalName, int iFatName, int iRippedName, float fWeightA, float fWeightB,
                                               float fWeightC)
{
    if (!pItemHandle || !iResult)
        return;

    ms_clothesGeometryStore.Add(pItemHandle, fWeightA, fWeightB, fWeightC, (RpGeometry*)iResult);
}

// Hook info
// Steals the function's single prologue instruction (sub esp,0x80).
#define HOOKPOS_CClothesBuilderBlendGeometry  0x5A4940
#define HOOKSIZE_CClothesBuilderBlendGeometry 6
DWORD                         RETURN_CClothesBuilderBlendGeometry = 0x5A4946;
static void __declspec(naked) HOOK_CClothesBuilderBlendGeometry()
{
    MTA_VERIFY_HOOK_LOCAL_SIZE;

    // clang-format off
    __asm
    {
        pushad
        push    [esp+32+4*7]
        push    [esp+32+4*7]
        push    [esp+32+4*7]
        push    [esp+32+4*7]
        push    [esp+32+4*7]
        push    [esp+32+4*7]
        push    [esp+32+4*7]
        call    OnCClothesBuilderBlendGeometryPre
        mov     [esp+0],eax
        add     esp, 4*7
        popad

        mov     eax,[esp-32-4*7]
        cmp     eax, 0
        jz      miss
        retn                        // Cache hit: eax already holds the geometry pointer

miss:
        push    [esp+0+4*7]
        push    [esp+0+4*7]
        push    [esp+0+4*7]
        push    [esp+0+4*7]
        push    [esp+0+4*7]
        push    [esp+0+4*7]
        push    [esp+0+4*7]
        call    inside
        add     esp, 4*7

        pushad
        push    [esp+32+4*7]
        push    [esp+32+4*7]
        push    [esp+32+4*7]
        push    [esp+32+4*7]
        push    [esp+32+4*7]
        push    [esp+32+4*7]
        push    [esp+32+4*7]
        push    eax
        call    OnCClothesBuilderBlendGeometryPost
        add     esp, 4*7+4
        popad
        retn

inside:
        // Original code
        sub     esp, 80h
        jmp     RETURN_CClothesBuilderBlendGeometry
    }
    // clang-format on
}

//////////////////////////////////////////////////////////////////////////////////////////
//
// CMultiplayerSA::InitHooks_ClothesGeometryCache
//
// Setup hooks for ClothesGeometryCache
//
//////////////////////////////////////////////////////////////////////////////////////////
void CMultiplayerSA::InitHooks_ClothesGeometryCache()
{
    EZHookInstall(CClothesBuilderBlendGeometry);
}
