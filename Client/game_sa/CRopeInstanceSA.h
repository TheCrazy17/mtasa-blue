/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto v1.0
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        game_sa/CRopeInstanceSA.h
 *  PURPOSE:     Standalone (non-pooled) SA rope instance
 *
 *  Multi Theft Auto is available from https://www.multitheftauto.com/
 *
 *****************************************************************************/

#pragma once

#include <CVector.h>

class CEntitySAInterface;

// Matches NUM_ROPE_SEGMENTS in gta-reversed's Rope.h - CRope::Render() always draws all 32, regardless
// of m_nSegments (how many are actively simulated).
constexpr uint8 ROPE_SEGMENT_COUNT = 32;

// Layout cross-checked against gta-reversed's `CRope` (source/game_sa/Rope.h), NOT MTA's older
// CRopesSAInterface in CRopesSA.h - the two disagree on what a few fields mean at the same byte
// offsets (m_pAttachedEntity/m_pRopeAttachObject are swapped, m_fSegmentLength is misidentified).
// gta-reversed is the more recent/rigorous reversing of the two, so we trust it here.
struct SRopeInstanceSA
{
    CVector             m_aSegments[32];             // 0x000 - chain node positions
    CVector             m_aSpeed[32];                // 0x180 - per-node velocity
    uint32              m_nId;                       // 0x300 - caller-chosen id, not a pointer
    float               m_fGroundZ;                  // 0x304
    float               m_fMass;                     // 0x308
    float               m_fTotalLength;               // 0x30C
    CEntitySAInterface* m_pRopeHolder;                // 0x310 - what's holding the rope (crane/object)
    CEntitySAInterface* m_pAttachedEntity;            // 0x314 - the hook/magnet prop itself
    CEntitySAInterface* m_pRopeAttachObject;          // 0x318 - the entity picked up by the hook, or null
    float               m_fSegmentLength;             // 0x31C
    uint32              m_nTime;                      // 0x320
    uint8               m_nSegments;                  // 0x324
    uint8               m_nType;                      // 0x325 - eRopeTypeSA
    uint8               m_nFlags1;                    // 0x326
    uint8               m_nFlags2;                    // 0x327
};
static_assert(sizeof(SRopeInstanceSA) == 0x328, "Invalid size for SRopeInstanceSA");

enum class eRopeTypeSA : uint8
{
    NONE = 0,
    CRANE_MAGNET1 = 1,
    CRANE_HARNESS = 2,
    MAGNET = 3,
    CRANE_MAGNO = 4,
    WRECKING_BALL = 5,
    QUARRY_CRANE_ARM = 6,
    CRANE_TROLLEY = 7,
    SWAT = 8,
};

// One rope, living outside the native 8-slot array (ms_aRopes). Created by borrowing the
// already-proven CreateRopeForSwatPed to get a fully native-initialized rope, relocating it to
// memory we own, then rewriting its type/holder before the first Update() runs. From then on,
// every operation (Update, Render, PickUpObject, ReleasePickedUpObject, Remove) calls the native
// function at its fixed address with `this` pointing at our own memory instead of a native slot -
// same __thiscall pattern CRopesSA already uses for RemoveEntityRope, just not limited to 8.
class CRopeInstanceSA
{
public:
    bool Create(eRopeTypeSA ropeType, const CVector& vecPosition, CEntitySAInterface* pHolder = nullptr);
    void Destroy();

    // Kept separate (not a combined Pulse()) because they must run at different points in the frame:
    // Update() is physics, safe to call from the logic tick. Render() calls straight into RenderWare
    // Im3D (RwIm3DTransform/RwRenderStateSet/...), which needs the camera/raster state RW sets up for
    // its own render pass - calling it from the logic tick crashes on relocated memory just as much as
    // on a native slot, because that per-frame RW state simply isn't there yet outside the render pass.
    void Update();            // call once per logic frame
    void Render();            // call once per render frame, from the same point the game renders native ropes

    bool PickUpObject(CEntitySAInterface* pEntity);
    void ReleasePickedUpObject();

    bool                 IsValid() const { return m_bValid; }
    CEntitySAInterface*  GetAttachedEntity() const { return m_instance.m_pRopeAttachObject; }
    eRopeTypeSA          GetRopeType() const { return static_cast<eRopeTypeSA>(m_instance.m_nType); }
    float                GetSegmentLength() const { return m_instance.m_fSegmentLength; }

    // m_fSegmentLength isn't a fixed "spacing" setting the way its name suggests - RegisterRope only
    // seeds it once (to 0.5 or 0.9, depending on holder type) as a starting point, then CRope::Update()
    // (0x557530) treats it as live physics state and keeps nudging it around every frame (wind/tension
    // pushes it, then clamps it back within a small range) - so a plain write here gets overwritten by
    // the very next Update() call and never visibly sticks. m_fPinnedSegmentLength/HasPinnedSegmentLength
    // hold what the script actually asked for; Update() re-asserts it AFTER each native call, every
    // frame, so it wins the fight with the native physics instead of losing it silently.
    void SetSegmentLength(float length)
    {
        m_instance.m_fSegmentLength = length;
        m_fPinnedSegmentLength = length;
        m_bHasPinnedSegmentLength = true;
    }

    void                 SetAnchorVelocity(const CVector& speed) { m_instance.m_aSpeed[0] = speed; }
    const CVector&       GetSegmentPosition(uint8 index) const { return m_instance.m_aSegments[index]; }

    // Live world position of the hook/magnet prop itself (not the fixed anchor point). The rope's own
    // segment simulation doesn't collide with world geometry, so a rope with no real holder can swing
    // or fall through the ground with nothing to stop it - this lets scripts monitor that themselves
    // (e.g. detach/destroy once it's below some height) instead of the native "fell into the void"
    // safety net doing it, which CRopesSA now skips entirely for pooled ropes (see CRopesSA.cpp).
    bool GetHookPosition(CVector& outPosition) const;

private:
    // Stand-in for a CPlaceable + CMatrixLink, used as m_pRopeHolder when the caller gives no real
    // holder entity. Crane/magnet-type ropes always hang from a real holder object in vanilla gameplay
    // (a crane), so CRope::Update() reads m_pRopeHolder as a CPlaceable in more than one place:
    //   - [holder + 0x14] (m_matrix): if set, some paths read position via *matrix + 0x30
    //     (CMatrix::m_pos); if null, they fall back to [holder + 0x4] (m_placement.m_vPosn) directly -
    //     matching CPlaceable::GetPosition()'s "m_matrix ? m_matrix->GetPosition() : m_placement.m_vPosn"
    //     (Placeable.h). That path degrades gracefully on a null matrix.
    //   - Other paths call straight into `CMatrix::TransformVector` (native Multiply3x3, 0x59C790 -
    //     see the doc comment on CMatrix::TransformVector in Matrix.h) with the holder's matrix as-is,
    //     reading m_right/m_forward/m_up (offsets 0x0/0x10/0x20) with NO null check at all - this one
    //     crashes outright on a null matrix.
    // So m_matrix has to point at a real (if inert) CMatrix: identity rotation, position pinned at the
    // rope's creation point, not attached to any RwMatrix (m_pAttachMatrix left null - nothing here
    // ever needs to sync to a renderable object). No virtual calls happen on the CPlaceable side either,
    // so a null vtable there is fine. No visible/world-registered game object is involved at all - this
    // is pure inert memory shaped like the real structs so the native code's assumptions hold.
    struct SFakeMatrix
    {
        CVector right{1.0f, 0.0f, 0.0f};             // 0x00
        uint32  flags = 0;                           // 0x0C
        CVector forward{0.0f, 1.0f, 0.0f};            // 0x10
        uint32  pad1 = 0;                             // 0x1C
        CVector up{0.0f, 0.0f, 1.0f};                 // 0x20
        uint32  pad2 = 0;                             // 0x2C
        CVector pos;                                  // 0x30
        uint32  pad3 = 0;                             // 0x3C
        void*   pAttachMatrix = nullptr;              // 0x40
        uint8   bOwnsAttachedMatrix = 0;               // 0x44
        uint8   pad4[3]{};                             // 0x45
        void*   pOwner = nullptr;                      // 0x48 - CMatrixLink::m_pOwner
        void*   pPrev = nullptr;                       // 0x4C
        void*   pNext = nullptr;                       // 0x50
    };
    static_assert(offsetof(SFakeMatrix, up) == 0x20 && offsetof(SFakeMatrix, pos) == 0x30 && sizeof(SFakeMatrix) == 0x54,
                  "SFakeMatrix must match CMatrixLink's native layout");

    struct SFakePlaceableHolder
    {
        void*      pVTable = nullptr;
        CVector    vecPosn;
        float      fHeading = 0.0f;
        SFakeMatrix* pMatrix = nullptr;
        SFakeMatrix  matrix;
    };
    static_assert(offsetof(SFakePlaceableHolder, vecPosn) == 0x4 && offsetof(SFakePlaceableHolder, pMatrix) == 0x14,
                  "SFakePlaceableHolder must match CPlaceable's native layout");

    SRopeInstanceSA       m_instance{};
    SFakePlaceableHolder  m_fakeHolder;
    bool                  m_bValid = false;
    float                 m_fPinnedSegmentLength = 0.0f;
    bool                  m_bHasPinnedSegmentLength = false;
};
