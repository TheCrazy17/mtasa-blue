/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto v1.0
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        sdk/game/COcclusion.h
 *  PURPOSE:     Occlusion zone interface
 *
 *  Multi Theft Auto is available from https://www.multitheftauto.com/
 *
 *****************************************************************************/

#pragma once
#include <CVector.h>
#include <cstddef>

// Position/size in world units, rotation in degrees; matches the convention every other
// MTA position/rotation getter/setter already uses, not the native fixed-point encoding.
struct SOcclusionZoneInfo
{
    CVector vecPosition;
    CVector vecSize;
    CVector vecRotation;
};

// Map and interior zones live in two separate native tables; flattened into one id space here
// so scripts only ever deal with a single number.
constexpr std::size_t OCCLUSION_INTERIOR_ID_BASE = 1000;

constexpr std::size_t MakeOcclusionId(std::size_t index, bool bInterior) noexcept
{
    return bInterior ? OCCLUSION_INTERIOR_ID_BASE + index : index;
}

class COcclusion
{
public:
    // Custom zones only; appended after every native zone in the same table.
    virtual bool CreateZone(const SOcclusionZoneInfo& info, bool bInterior, std::size_t& outIndex) = 0;

    virtual bool GetZoneData(std::size_t index, bool bInterior, SOcclusionZoneInfo& outInfo) = 0;
    virtual bool SetZoneData(std::size_t index, bool bInterior, const SOcclusionZoneInfo& info) = 0;

    virtual std::size_t GetZoneCount(bool bInterior) const = 0;

    // Native zones only, identified by the flattened id above. Disabling shrinks the zone to a
    // near-zero box in place, so it never touches the table's index or count.
    virtual bool IsZoneEnabled(std::size_t id) const = 0;
    virtual bool SetZoneEnabled(std::size_t id, bool bEnabled, void* pChangeSource) = 0;

    // Undoes every native zone change made through the given source, e.g. a stopping resource.
    virtual void UndoChanges(void* pChangeSource) = 0;

    // Hard reset: drops every custom zone and restores every native zone, regardless of source.
    virtual void Reset() = 0;
};
