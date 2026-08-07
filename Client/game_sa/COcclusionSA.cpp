/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto v1.0
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        game_sa/COcclusionSA.cpp
 *  PURPOSE:     Occlusion zone implementation
 *
 *  Multi Theft Auto is available from https://www.multitheftauto.com/
 *
 *****************************************************************************/

#include "StdInc.h"
#include "COcclusionSA.h"

namespace
{
    using AddOne_t = void(__cdecl*)(float, float, float, float, float, float, float, float, float, std::uint32_t, bool);

    // Position/size are stored as (value * 4) in a signed 16-bit field.
    constexpr float FIXED_POS_SCALE = 4.0f;

    // A disabled zone is shrunk down to this instead of being removed from the table; the table
    // has no concept of a hole, so shrinking is the only way to make a zone stop occluding.
    const CVector DISABLED_ZONE_SIZE(0.01f, 0.01f, 0.01f);

    // Rotation covers a full circle in one byte, with +180 degrees baked in by the engine
    // before encoding (see COcclusion::AddOne in the reversed engine); round-tripping a
    // getter/setter pair through this must apply and remove that same offset.
    std::uint8_t EncodeRotation(float fDegrees)
    {
        float fNormalized = std::fmod(fDegrees + 180.0f, 360.0f);
        if (fNormalized < 0.0f)
            fNormalized += 360.0f;
        return static_cast<std::uint8_t>(std::lround(fNormalized * (256.0f / 360.0f))) & 0xFF;
    }

    float DecodeRotation(std::uint8_t ucRaw)
    {
        return ucRaw * (360.0f / 256.0f) - 180.0f;
    }
}  // namespace

COcclusionSA::COcclusionSA() : m_uiNativeMapCount(0), m_uiNativeInteriorCount(0), m_bBoundaryCaptured(false)
{
}

SOccluderSAInterface* COcclusionSA::GetTable(bool bInterior)
{
    return reinterpret_cast<SOccluderSAInterface*>(bInterior ? VAR_COcclusion_InteriorOccluders : VAR_COcclusion_Occluders);
}

std::uint32_t& COcclusionSA::GetCount(bool bInterior)
{
    return *reinterpret_cast<std::uint32_t*>(bInterior ? VAR_COcclusion_NumInteriorOccluders : VAR_COcclusion_NumOccludersOnMap);
}

std::size_t COcclusionSA::GetMaxCount(bool bInterior)
{
    return bInterior ? MAX_INTERIOR_OCCLUDERS : MAX_MAP_OCCLUDERS;
}

std::size_t COcclusionSA::GetZoneCount(bool bInterior) const
{
    return GetCount(bInterior);
}

bool COcclusionSA::CreateZone(const SOcclusionZoneInfo& info, bool bInterior, std::size_t& outIndex)
{
    if (GetCount(bInterior) >= GetMaxCount(bInterior))
        return false;

    // Native param order is (width, length, height); vecSize.fX/fY follow the same width/length
    // convention as the reversed engine's own COccluder::GetSize() accessor (see SetZoneData).
    auto* const addOne = reinterpret_cast<AddOne_t>(FUNC_COcclusion_AddOne);
    addOne(info.vecPosition.fX, info.vecPosition.fY, info.vecPosition.fZ, info.vecSize.fX, info.vecSize.fY, info.vecSize.fZ, info.vecRotation.fZ,
           info.vecRotation.fY, info.vecRotation.fX, 0, bInterior);

    const std::uint32_t uiCount = GetCount(bInterior);
    if (uiCount == 0)
        return false;  // AddOne rejects all-zero-size zones without appending anything

    outIndex = uiCount - 1;
    return true;
}

bool COcclusionSA::GetZoneData(std::size_t index, bool bInterior, SOcclusionZoneInfo& outInfo)
{
    if (index >= GetCount(bInterior))
        return false;

    const SOccluderSAInterface& occluder = GetTable(bInterior)[index];
    outInfo.vecPosition = CVector(occluder.sCenterX / FIXED_POS_SCALE, occluder.sCenterY / FIXED_POS_SCALE, occluder.sCenterZ / FIXED_POS_SCALE);
    outInfo.vecSize = CVector(occluder.sWidth / FIXED_POS_SCALE, occluder.sLength / FIXED_POS_SCALE, occluder.sHeight / FIXED_POS_SCALE);
    outInfo.vecRotation = CVector(DecodeRotation(occluder.ucRotX), DecodeRotation(occluder.ucRotY), DecodeRotation(occluder.ucRotZ));
    return true;
}

bool COcclusionSA::SetZoneData(std::size_t index, bool bInterior, const SOcclusionZoneInfo& info)
{
    if (index >= GetCount(bInterior))
        return false;

    SOccluderSAInterface& occluder = GetTable(bInterior)[index];
    occluder.sCenterX = static_cast<std::int16_t>(std::lround(info.vecPosition.fX * FIXED_POS_SCALE));
    occluder.sCenterY = static_cast<std::int16_t>(std::lround(info.vecPosition.fY * FIXED_POS_SCALE));
    occluder.sCenterZ = static_cast<std::int16_t>(std::lround(info.vecPosition.fZ * FIXED_POS_SCALE));
    occluder.sWidth = static_cast<std::int16_t>(std::lround(info.vecSize.fX * FIXED_POS_SCALE));
    occluder.sLength = static_cast<std::int16_t>(std::lround(info.vecSize.fY * FIXED_POS_SCALE));
    occluder.sHeight = static_cast<std::int16_t>(std::lround(info.vecSize.fZ * FIXED_POS_SCALE));
    occluder.ucRotX = EncodeRotation(info.vecRotation.fX);
    occluder.ucRotY = EncodeRotation(info.vecRotation.fY);
    occluder.ucRotZ = EncodeRotation(info.vecRotation.fZ);
    // usNextIndexAndFlags is left untouched; it threads this entry into the engine's live bucket list.
    return true;
}

bool COcclusionSA::IsZoneEnabled(std::size_t id) const
{
    auto it = m_Disablers.find(id);
    return it == m_Disablers.end() || it->second.empty();
}

bool COcclusionSA::SetZoneEnabled(std::size_t id, bool bEnabled, void* pChangeSource)
{
    const bool        bInterior = id >= OCCLUSION_INTERIOR_ID_BASE;
    const std::size_t index = bInterior ? id - OCCLUSION_INTERIOR_ID_BASE : id;
    if (index >= GetCount(bInterior))
        return false;

    if (bEnabled)
    {
        auto it = m_Disablers.find(id);
        if (it == m_Disablers.end())
            return true;  // already enabled

        it->second.erase(pChangeSource);
        if (!it->second.empty())
            return true;  // another source still wants this zone disabled

        m_Disablers.erase(it);

        SOcclusionZoneInfo info;
        GetZoneData(index, bInterior, info);
        info.vecSize = m_PristineSize[id];
        SetZoneData(index, bInterior, info);
        m_PristineSize.erase(id);
        return true;
    }

    std::set<void*>& disablers = m_Disablers[id];
    if (disablers.empty())
    {
        SOcclusionZoneInfo info;
        GetZoneData(index, bInterior, info);
        m_PristineSize[id] = info.vecSize;

        info.vecSize = DISABLED_ZONE_SIZE;
        SetZoneData(index, bInterior, info);
    }
    disablers.insert(pChangeSource);
    return true;
}

void COcclusionSA::UndoChanges(void* pChangeSource)
{
    for (auto it = m_Disablers.begin(); it != m_Disablers.end();)
    {
        std::set<void*>& disablers = it->second;
        disablers.erase(pChangeSource);
        if (!disablers.empty())
        {
            ++it;
            continue;
        }

        const std::size_t id = it->first;
        const bool        bInterior = id >= OCCLUSION_INTERIOR_ID_BASE;
        const std::size_t index = bInterior ? id - OCCLUSION_INTERIOR_ID_BASE : id;

        if (index < GetCount(bInterior))
        {
            SOcclusionZoneInfo info;
            GetZoneData(index, bInterior, info);
            info.vecSize = m_PristineSize[id];
            SetZoneData(index, bInterior, info);
        }
        m_PristineSize.erase(id);
        it = m_Disablers.erase(it);
    }
}

void COcclusionSA::Reset()
{
    if (!m_bBoundaryCaptured)
    {
        // First call happens once the native tables are fully loaded and before any script has
        // had a chance to append a custom zone; that count is the permanent native/custom boundary.
        m_uiNativeMapCount = GetCount(false);
        m_uiNativeInteriorCount = GetCount(true);
        m_bBoundaryCaptured = true;
        return;
    }

    for (const auto& [id, vecSize] : m_PristineSize)
    {
        const bool        bInterior = id >= OCCLUSION_INTERIOR_ID_BASE;
        const std::size_t index = bInterior ? id - OCCLUSION_INTERIOR_ID_BASE : id;
        if (index >= GetCount(bInterior))
            continue;

        SOcclusionZoneInfo info;
        GetZoneData(index, bInterior, info);
        info.vecSize = vecSize;
        SetZoneData(index, bInterior, info);
    }

    m_Disablers.clear();
    m_PristineSize.clear();

    // Drops every custom zone; they only ever live past the native boundary.
    GetCount(false) = static_cast<std::uint32_t>(m_uiNativeMapCount);
    GetCount(true) = static_cast<std::uint32_t>(m_uiNativeInteriorCount);
}
