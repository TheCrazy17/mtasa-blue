/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto v1.0
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        game_sa/CRendererSA.cpp
 *  PURPOSE:     Game renderer class
 *
 *  Multi Theft Auto is available from https://www.multitheftauto.com/
 *
 *****************************************************************************/

#include "StdInc.h"
#include "CRendererSA.h"
#include "CGameSA.h"
#include "CModelInfoSA.h"
#include "CMatrix.h"
#include "gamesa_renderware.h"

extern CGameSA* pGame;

#define SetLightColoursForPedsCarsAndObjects(fMult) ((RpLight * (__cdecl*)(float))0x735D90)(fMult)
#define SetAmbientColours()                         ((RpLight * (__cdecl*)())0x735D30)()

using RpMaterialUVAnimCallback_t = RpMaterial*(__cdecl*)(RpMaterial*, void*);
#define RpGeometryForAllMaterials(geometry, callback, data) \
    ((RpGeometry * (__cdecl*)(RpGeometry*, RpMaterialUVAnimCallback_t, void*))0x74C790)(geometry, callback, data)
#define MaterialUpdateUVAnimCB ((RpMaterialUVAnimCallback_t)0x532D70)

// Tracks the last frame each model's UV animation was advanced through RenderModel, so drawing the
// same model several times in one frame (e.g. several onClientPreRender handlers) only advances it
// once instead of once per draw.
static std::unordered_map<CBaseModelInfoSAInterface*, int> ms_LastUVAnimFrame;

CRendererSA::CRendererSA()
{
}

CRendererSA::~CRendererSA()
{
}

void CRendererSA::RenderModel(CModelInfo* pModelInfo, const CMatrix& matrix, float lighting)
{
    CBaseModelInfoSAInterface* pModelInfoSAInterface = pModelInfo->GetInterface();
    if (!pModelInfoSAInterface)
        return;

    RwObject* pRwObject = pModelInfoSAInterface->pRwObject;
    if (!pRwObject)
        return;

    RwFrame* pFrame = RpGetFrame(pRwObject);

    static RwMatrix rwMatrix;
    rwMatrix.right = (RwV3d&)matrix.vRight;
    rwMatrix.up = (RwV3d&)matrix.vFront;
    rwMatrix.at = (RwV3d&)matrix.vUp;
    rwMatrix.pos = (RwV3d&)matrix.vPos;
    RwFrameTransform(pFrame, &rwMatrix, rwCOMBINEREPLACE);

    // Setup ambient light multiplier
    SetLightColoursForPedsCarsAndObjects(lighting);

    if (pRwObject->type == RP_TYPE_ATOMIC)
    {
        RpAtomic* pRpAtomic = reinterpret_cast<RpAtomic*>(pRwObject);

        // This bypasses CEntity::PreRender, the only place that normally advances UV animated
        // textures, so do it here too. bHasBeenPreRendered doubles as an "already advanced this
        // frame" flag: true means a real entity of this model got there first via its own
        // PreRender, so skip; either way leave it false so next frame resets cleanly.
        if (pRpAtomic->geometry)
        {
            int  frameCounter = pGame->GetSystemFrameCounter();
            int& lastFrame = ms_LastUVAnimFrame[pModelInfoSAInterface];
            if (lastFrame != frameCounter && !pModelInfoSAInterface->bHasBeenPreRendered)
                RpGeometryForAllMaterials(pRpAtomic->geometry, MaterialUpdateUVAnimCB, nullptr);
            pModelInfoSAInterface->bHasBeenPreRendered = false;
            lastFrame = frameCounter;
        }

        pRpAtomic->renderCallback(reinterpret_cast<RpAtomic*>(pRwObject));
    }
    else
    {
        RpClump* pClump = reinterpret_cast<RpClump*>(pRwObject);
        RpClumpRender(pClump);
    }

    // Restore ambient light
    SetAmbientColours();
}
