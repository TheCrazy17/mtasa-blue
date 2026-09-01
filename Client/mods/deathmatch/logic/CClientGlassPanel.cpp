/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto v1.0
 *               (Shared logic for modifications)
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        mods/shared_logic/CClientGlassPanel.cpp
 *  PURPOSE:     Glass panel entity class
 *
 *****************************************************************************/

#include <StdInc.h>

namespace
{
    // Smallest form of the classic COLL file format that CRenderWareSA::ReadCOL already parses;
    // just a bounding sphere/box and a single box shape, no spheres, lines, mesh vertices or faces.
    // Building one of these and handing it to the existing file parser is far less risky than
    // constructing a CColModel by hand, since the parser is already tested against real .col data.
#pragma pack(push, 1)
    struct SColFileHeader
    {
        char     szVersion[4];
        uint32_t uiSize;
        char     szName[24];
    };

    struct SColSurface
    {
        uint8_t ucMaterial, ucPiece, ucBrightness, ucLighting;
    };

    struct SColBounds
    {
        float fRadius, fCenterX, fCenterY, fCenterZ, fMinX, fMinY, fMinZ, fMaxX, fMaxY, fMaxZ;
    };

    struct SColBox
    {
        float       fMinX, fMinY, fMinZ, fMaxX, fMaxY, fMaxZ;
        SColSurface surface;
    };
#pragma pack(pop)

    // Box extents are in the entity's own local space, where the identity matrix's right/front/up
    // axes are +X/+Y/+Z; width runs along right (X), thickness along front (Y), height along up (Z).
    CColModel* BuildBoxColModel(float fWidth, float fHeight, float fThickness)
    {
        const float fHalfWidth = std::max(fWidth, 0.05f) * 0.5f;
        const float fHalfHeight = std::max(fHeight, 0.05f) * 0.5f;
        const float fHalfThickness = std::max(fThickness, 0.05f) * 0.5f;

        SColBounds bounds{};
        bounds.fRadius = sqrtf(fHalfWidth * fHalfWidth + fHalfHeight * fHalfHeight + fHalfThickness * fHalfThickness);
        bounds.fMinX = -fHalfWidth;
        bounds.fMinY = -fHalfThickness;
        bounds.fMinZ = -fHalfHeight;
        bounds.fMaxX = fHalfWidth;
        bounds.fMaxY = fHalfThickness;
        bounds.fMaxZ = fHalfHeight;

        SColBox box{};
        box.fMinX = bounds.fMinX;
        box.fMinY = bounds.fMinY;
        box.fMinZ = bounds.fMinZ;
        box.fMaxX = bounds.fMaxX;
        box.fMaxY = bounds.fMaxY;
        box.fMaxZ = bounds.fMaxZ;

        const uint32_t uiZero = 0;
        const uint32_t uiOne = 1;

        SString strPayload;
        strPayload.append(reinterpret_cast<const char*>(&bounds), sizeof(bounds));
        strPayload.append(reinterpret_cast<const char*>(&uiZero), sizeof(uiZero));            // sphere count
        strPayload.append(reinterpret_cast<const char*>(&uiZero), sizeof(uiZero));            // line count
        strPayload.append(reinterpret_cast<const char*>(&uiOne), sizeof(uiOne));               // box count
        strPayload.append(reinterpret_cast<const char*>(&box), sizeof(box));
        strPayload.append(reinterpret_cast<const char*>(&uiZero), sizeof(uiZero));            // vertex count
        strPayload.append(reinterpret_cast<const char*>(&uiZero), sizeof(uiZero));            // triangle count

        SColFileHeader header{};
        memcpy(header.szVersion, "COLL", 4);
        memcpy(header.szName, "glasspanel", 10);
        header.uiSize = static_cast<uint32_t>(sizeof(header) - 8 + strPayload.length());

        SString strBuffer;
        strBuffer.append(reinterpret_cast<const char*>(&header), sizeof(header));
        strBuffer.append(strPayload);

        return g_pGame->GetRenderWare()->ReadCOL(strBuffer);
    }
}            // namespace

CClientGlassPanel::CClientGlassPanel(CClientManager* pManager, ElementID ID) : ClassInit(this), CClientEntity(ID)
{
    m_pManager = pManager;
    m_pGlassPanelManager = pManager->GetGlassPanelManager();

    m_vecRotation = CVector();
    m_fWidth = 1.0f;
    m_fHeight = 1.0f;
    m_fThickness = 0.02f;
    m_Color = SColor(0x60FFFFFF);
    m_bBreakable = false;
    m_bBroken = false;
    m_ucDamage = 0;
    m_ucMaxDamage = 0;
    m_bCollisionEnabled = false;
    m_pCollisionObject = nullptr;

    SetTypeName("glass-panel");

    m_pGlassPanelManager->AddToList(this);
}

CClientGlassPanel::~CClientGlassPanel()
{
    SetCollisionEnabled(false);
    Unlink();
}

void CClientGlassPanel::Unlink()
{
    m_pGlassPanelManager->RemoveFromList(this);
}

void CClientGlassPanel::SetPosition(const CVector& vecPosition)
{
    m_vecPosition = vecPosition;
    UpdateCollision();
}

void CClientGlassPanel::SetRotationRadians(const CVector& vecRadians)
{
    m_vecRotation = vecRadians;
    UpdateCollision();
}

void CClientGlassPanel::SetSize(float fWidth, float fHeight)
{
    m_fWidth = fWidth;
    m_fHeight = fHeight;
    UpdateCollision();
}

void CClientGlassPanel::DoPulse()
{
    if (m_bBroken)
        return;

    CMatrix matrix;
    GetMatrix(matrix);

    const CVector vecRight = matrix.vRight * (m_fWidth * 0.5f);
    const CVector vecUp = matrix.vUp * (m_fHeight * 0.5f);
    const CVector vecNormal = matrix.vFront * (m_fThickness * 0.5f);

    // Bottom-left, top-left, top-right, bottom-right of the panel's middle plane
    const CVector corners[4] = {
        matrix.vPos - vecRight - vecUp,
        matrix.vPos - vecRight + vecUp,
        matrix.vPos + vecRight + vecUp,
        matrix.vPos + vecRight - vecUp,
    };

    // Each point of accumulated damage whitens and thickens the panel a bit, standing in for a
    // frosted crack pattern without needing a separate crack texture or geometry.
    const float    fDamageRatio = m_ucMaxDamage > 0 ? static_cast<float>(m_ucDamage) / static_cast<float>(m_ucMaxDamage) : 0.0f;
    const auto     LerpToWhite = [fDamageRatio](unsigned char ucChannel) { return static_cast<unsigned char>(ucChannel + (255 - ucChannel) * fDamageRatio); };
    const D3DCOLOR color = D3DCOLOR_ARGB(LerpToWhite(m_Color.A), LerpToWhite(m_Color.R), LerpToWhite(m_Color.G), LerpToWhite(m_Color.B));

    // Queued draws take ownership of a heap allocation and consume it later in the frame, so this
    // cannot be a stack-local vector.
    auto pVecVertices = new std::vector<PrimitiveVertice>();
    pVecVertices->reserve(24);

    // Front and back faces, offset along the panel's normal by half the thickness. Each face is
    // pushed in with both triangle windings so the panel reads as glass from either side no matter
    // which way this render stage culls, rather than needing to disable culling for the draw.
    for (const CVector& vecOffset : {vecNormal, -vecNormal})
    {
        CVector facePositions[4];
        for (int i = 0; i < 4; i++)
            facePositions[i] = corners[i] + vecOffset;

        const int windings[2][6] = {
            {0, 1, 2, 0, 2, 3},
            {2, 1, 0, 3, 2, 0},
        };

        for (const auto& winding : windings)
        {
            for (int index : winding)
                pVecVertices->push_back(PrimitiveVertice{facePositions[index].fX, facePositions[index].fY, facePositions[index].fZ, color});
        }
    }

    if (g_pCore->GetGraphics()->IsValidPrimitiveSize(pVecVertices->size(), D3DPT_TRIANGLELIST))
        g_pCore->GetGraphics()->DrawPrimitive3DQueued(pVecVertices, D3DPT_TRIANGLELIST);
    else
        delete pVecVertices;
}

bool CClientGlassPanel::SetCollisionEnabled(bool bEnabled)
{
    m_bCollisionEnabled = bEnabled;
    UpdateCollision();
    return true;
}

bool CClientGlassPanel::Break(const CVector& vecForce, unsigned char ucGranularity)
{
    if (m_bBroken || !m_bBreakable)
        return false;

    m_bBroken = true;
    SetCollisionEnabled(false);

    CMatrix matrix;
    GetMatrix(matrix);

    const CVector vecRight = matrix.vRight * m_fWidth;
    const CVector vecUp = matrix.vUp * m_fHeight;
    const CVector vecCorner = matrix.vPos - vecRight * 0.5f - vecUp * 0.5f;

    g_pMultiplayer->ShatterGlassPanel(vecCorner, vecUp, vecRight, vecForce, matrix.vPos, ucGranularity);
    return true;
}

bool CClientGlassPanel::Damage(unsigned char ucAmount, const CVector& vecForce, unsigned char ucGranularity)
{
    if (m_bBroken || !m_bBreakable || m_ucMaxDamage == 0)
        return false;

    m_ucDamage = static_cast<unsigned char>(std::min<int>(m_ucDamage + ucAmount, m_ucMaxDamage));

    if (m_ucDamage >= m_ucMaxDamage)
        Break(vecForce, ucGranularity);

    return true;
}

void CClientGlassPanel::UpdateCollision()
{
    if (!m_bCollisionEnabled || m_bBroken)
    {
        if (m_pCollisionObject)
        {
            const unsigned short          usModel = m_pCollisionObject->GetModel();
            std::shared_ptr<CClientModel> pModel = m_pManager->GetModelManager()->FindModelByID(usModel);

            delete m_pCollisionObject;
            m_pCollisionObject = nullptr;

            if (pModel)
                m_pManager->GetModelManager()->Remove(pModel);
        }
        return;
    }

    if (!m_pCollisionObject)
    {
        // Hidden immediately below, so the geometry this parent model provides is never seen; it
        // only exists so the custom model slot starts from a known-valid state.
        constexpr unsigned short GLASS_PANEL_COLLISION_PARENT_MODEL = 1337;

        CClientModelManager* pModelManager = m_pManager->GetModelManager();
        const int            iModelID = pModelManager->GetFirstFreeModelID();
        if (iModelID == INVALID_MODEL_ID)
            return;

        std::shared_ptr<CClientModel> pModel = pModelManager->Request(m_pManager, iModelID, eClientModelType::OBJECT);
        if (!pModel || !pModel->Allocate(GLASS_PANEL_COLLISION_PARENT_MODEL))
        {
            if (pModel)
                pModelManager->Remove(pModel);
            return;
        }
        pModelManager->Add(pModel);

        m_pCollisionObject = new CClientObject(m_pManager, INVALID_ELEMENT_ID, static_cast<unsigned short>(iModelID), false);
        m_pCollisionObject->SetVisible(false);
    }

    m_pCollisionObject->SetPosition(m_vecPosition);
    m_pCollisionObject->SetRotationRadians(m_vecRotation);

    CColModel* pColModel = BuildBoxColModel(m_fWidth, m_fHeight, m_fThickness);
    if (pColModel)
    {
        CModelInfo* pModelInfo = g_pGame->GetModelInfo(m_pCollisionObject->GetModel());
        if (pModelInfo)
            pModelInfo->SetColModel(pColModel);
    }
}
