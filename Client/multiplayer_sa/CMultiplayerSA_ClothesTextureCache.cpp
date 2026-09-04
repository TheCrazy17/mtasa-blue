/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto v1.0
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        multiplayer_sa/CMultiplayerSA_ClothesTextureCache.cpp
 *  PURPOSE:     Cache converted clothes textures to reduce stutter when changing clothes
 *
 *  Multi Theft Auto is available from https://www.multitheftauto.com/
 *
 *****************************************************************************/

#include "StdInc.h"
#include "..\game_sa\gamesa_renderware.h"

// CClothesBuilder::LoadAndPutOnClothes, called once per clothes texture slot (torso, legs,
// face, feet, necklace, watch, glasses, hat, suit...) from CClothesBuilder::ConstructTextures.
// On the branch we care about, it streams in that item's TXD, fetches its first texture and
// makes a working copy of it (CopyTexture). That streamed TXD load is what actually pays for
// CStreamingConvertBufferToObject's decompression work, on every single clothes change, even
// when the item being requested was already worn earlier in the same session. This hook caches
// the resulting texture, keyed by the item's own texture key (the same stable per-slot value
// CPedClothesDesc already uses for the whole-outfit cache in CMultiplayerSA_ClothesCache.cpp),
// so a repeat of the same item skips the streaming load and CopyTexture entirely.
//
// This does not cover the accessory decal chain inside ConstructTextures (the loop that blends
// Left_upper_arm/Left_lower_arm/etc onto each other in sequence, threading the previous result
// into the next call's first argument). Each of those calls depends on everything blended into
// it so far, not just its own texture key, so caching them individually would be wrong. That
// loop is left alone; only the plain "fresh copy" slots are cached.
//
// Reference counting: RwTexture already carries its own refs count (RenderWare.h). Attaching a
// texture to a material (RpMaterialSetTexture, 0x74dbc0) takes its own ref and releases whatever
// was there before via RwTextureDestroy (0x7f3820), which decrements refs and only actually frees
// the texture once refs drops to zero. RwTexDictionaryAddTexture (0x7f3980) only links the texture
// into the dictionary's list; it does not touch refs. So a texture handed back by
// LoadAndPutOnClothes starts out owned solely by its creator. This cache takes its own extra ref
// before holding on to a texture past the call that made it, and drops that ref through
// RwTextureDestroy when the entry is evicted or the cache is flushed, so it never depends on
// what the rest of the clothes builder does with its own references.
RwTexture* _cdecl OnCClothesBuilderLoadAndPutOnClothesPre(int iExistingTexture, int iStreamingId, unsigned int uiTextureKey, unsigned int* puiResolvedId);
void _cdecl       OnCClothesBuilderLoadAndPutOnClothesPost(RwTexture* pResult, int iExistingTexture, int iStreamingId, unsigned int uiTextureKey,
                                                           unsigned int* puiResolvedId);

////////////////////////////////////////////////
//
// class CClothesTextureStore
//
// Save converted clothes textures for later, keyed by their clothes texture key
//
////////////////////////////////////////////////
class CClothesTextureStore
{
public:
    struct SCachedTextureInfo
    {
        unsigned int uiTextureKey;
        RwTexture*   pTexture;
        unsigned int uiResolvedId;
    };

    std::vector<SCachedTextureInfo> cachedList;
    uint                            m_uiMaxSize = 64;

    ///////////////////////////////////////
    //
    // Find a texture matching the given key. Adds our own ref on hit, as the caller is
    // going to treat the result exactly like a freshly created texture.
    //
    ///////////////////////////////////////
    RwTexture* FindMatchAndUse(unsigned int uiTextureKey, unsigned int* puiOutResolvedId)
    {
        for (SCachedTextureInfo& info : cachedList)
        {
            if (info.uiTextureKey == uiTextureKey)
            {
                info.pTexture->refs++;
                *puiOutResolvedId = info.uiResolvedId;
                return info.pTexture;
            }
        }
        return nullptr;
    }

    ///////////////////////////////////////
    //
    // Save a ref to this texture
    //
    ///////////////////////////////////////
    void Add(unsigned int uiTextureKey, RwTexture* pTexture, unsigned int uiResolvedId)
    {
        if (!pTexture)
            return;

        for (const SCachedTextureInfo& info : cachedList)
            if (info.uiTextureKey == uiTextureKey)
                return;  // Already cached (shouldn't normally happen)

        if (cachedList.size() >= m_uiMaxSize)
        {
            RwTextureDestroy(cachedList.front().pTexture);
            cachedList.erase(cachedList.begin());
        }

        pTexture->refs++;
        cachedList.push_back({uiTextureKey, pTexture, uiResolvedId});
    }

    ///////////////////////////////////////
    //
    // Drop every cached texture. Used when custom clothes textures are added or removed,
    // same trigger as CMultiplayerSA::FlushClothesCache.
    //
    ///////////////////////////////////////
    void Clear()
    {
        for (const SCachedTextureInfo& info : cachedList)
            RwTextureDestroy(info.pTexture);
        cachedList.clear();
    }
};

CClothesTextureStore ms_clothesTextureStore;

////////////////////////////////////////////////
//
// ClearClothesTextureCache
//
// Called from CMultiplayerSA::FlushClothesCache so this cache never outlives a change to the
// underlying clothes textures.
//
////////////////////////////////////////////////
void ClearClothesTextureCache()
{
    ms_clothesTextureStore.Clear();
}

////////////////////////////////////////////////
//
// Hook CClothesBuilder::LoadAndPutOnClothes
//
//
////////////////////////////////////////////////
RwTexture* _cdecl OnCClothesBuilderLoadAndPutOnClothesPre(int iExistingTexture, int iStreamingId, unsigned int uiTextureKey, unsigned int* puiResolvedId)
{
    // Only cache the plain "fresh copy" path: no existing texture to blend onto, a real
    // streaming id, and a real (non-zero) texture key.
    if (iExistingTexture != 0 || iStreamingId == -1 || uiTextureKey == 0)
        return nullptr;

    return ms_clothesTextureStore.FindMatchAndUse(uiTextureKey, puiResolvedId);
}

////////////////////////////////////////////////////////////////////////////////////////////////
//
void _cdecl OnCClothesBuilderLoadAndPutOnClothesPost(RwTexture* pResult, int iExistingTexture, int iStreamingId, unsigned int uiTextureKey,
                                                     unsigned int* puiResolvedId)
{
    if (iExistingTexture != 0 || iStreamingId == -1 || uiTextureKey == 0 || !pResult)
        return;

    ms_clothesTextureStore.Add(uiTextureKey, pResult, *puiResolvedId);
}

// Hook info
// Steals the function's first 3 instructions (push edi ; mov edi,[esp+0xc] ; cmp edi,-1),
// stopping right before the jz that follows -- that jump stays where it is and still resolves
// correctly since we never move it.
#define HOOKPOS_CClothesBuilderLoadAndPutOnClothes  0x5A5F70
#define HOOKSIZE_CClothesBuilderLoadAndPutOnClothes 8
DWORD                         RETURN_CClothesBuilderLoadAndPutOnClothes = 0x5A5F78;
static void __declspec(naked) HOOK_CClothesBuilderLoadAndPutOnClothes()
{
    MTA_VERIFY_HOOK_LOCAL_SIZE;

    // clang-format off
    __asm
    {
        pushad
        push    [esp+32+4*4]
        push    [esp+32+4*4]
        push    [esp+32+4*4]
        push    [esp+32+4*4]
        call    OnCClothesBuilderLoadAndPutOnClothesPre
        mov     [esp+0],eax
        add     esp, 4*4
        popad

        mov     eax,[esp-32-4*4]
        cmp     eax, 0
        jz      miss
        retn                        // Cache hit: eax already holds the texture pointer

miss:
        push    [esp+0+4*4]
        push    [esp+0+4*4]
        push    [esp+0+4*4]
        push    [esp+0+4*4]
        call    inside
        add     esp, 4*4

        pushad
        push    [esp+32+4*4]
        push    [esp+32+4*4]
        push    [esp+32+4*4]
        push    [esp+32+4*4]
        push    eax
        call    OnCClothesBuilderLoadAndPutOnClothesPost
        add     esp, 4*4+4
        popad
        retn

inside:
        // Original code
        push    edi
        mov     edi,[esp+0xc]
        cmp     edi,-1
        jmp     RETURN_CClothesBuilderLoadAndPutOnClothes
    }
    // clang-format on
}

//////////////////////////////////////////////////////////////////////////////////////////
//
// CMultiplayerSA::InitHooks_ClothesTextureCache
//
// Setup hooks for ClothesTextureCache
//
//////////////////////////////////////////////////////////////////////////////////////////
void CMultiplayerSA::InitHooks_ClothesTextureCache()
{
    EZHookInstall(CClothesBuilderLoadAndPutOnClothes);
}
