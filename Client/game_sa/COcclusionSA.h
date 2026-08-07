/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto v1.0
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        game_sa/COcclusionSA.h
 *  PURPOSE:     Occlusion zone implementation
 *
 *  Multi Theft Auto is available from https://www.multitheftauto.com/
 *
 *****************************************************************************/

#pragma once

#include <map>
#include <set>
#include <game/COcclusion.h>
#include "COccluderSA.h"

class COcclusionSA final : public COcclusion
{
public:
    COcclusionSA();

    bool CreateZone(const SOcclusionZoneInfo& info, bool bInterior, std::size_t& outIndex) override;

    bool GetZoneData(std::size_t index, bool bInterior, SOcclusionZoneInfo& outInfo) override;
    bool SetZoneData(std::size_t index, bool bInterior, const SOcclusionZoneInfo& info) override;

    std::size_t GetZoneCount(bool bInterior) const override;

    bool IsZoneEnabled(std::size_t id) const override;
    bool SetZoneEnabled(std::size_t id, bool bEnabled, void* pChangeSource) override;

    void UndoChanges(void* pChangeSource) override;
    void Reset() override;

private:
    static SOccluderSAInterface* GetTable(bool bInterior);
    static std::uint32_t&        GetCount(bool bInterior);
    static std::size_t           GetMaxCount(bool bInterior);

    // Sources currently holding a native zone disabled, keyed by flattened id; the zone stays
    // disabled as long as this is non-empty, so one resource can't re-enable what another wants off.
    std::map<std::size_t, std::set<void*>> m_Disablers;

    // Size a native zone had right before its first disable, so it can be restored exactly;
    // filled in lazily since most zones are never touched.
    std::map<std::size_t, CVector> m_PristineSize;

    std::size_t m_uiNativeMapCount;
    std::size_t m_uiNativeInteriorCount;
    bool        m_bBoundaryCaptured;
};
