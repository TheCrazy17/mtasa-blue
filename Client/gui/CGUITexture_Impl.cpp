/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto v1.0
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        gui/CGUITexture_Impl.cpp
 *  PURPOSE:     Texture handling class
 *
 *  Multi Theft Auto is available from https://www.multitheftauto.com/
 *
 *****************************************************************************/

#include "StdInc.h"
#include <CEGUI/RendererModules/Direct3D9/Texture.h>

CGUITexture_Impl::CGUITexture_Impl(CGUI_Impl* pGUI)
{
    // Save the renderer
    m_pRenderer = pGUI->GetRenderer();

    // CEGUI 0.8.7 textures are named and tracked by the renderer, unlike the anonymous
    // textures the old CEGUI 0.4 renderer created.
    char szUnique[CGUI_CHAR_SIZE];
    pGUI->GetUniqueName(szUnique);
    m_strTextureName = szUnique;

    // Create the texture
    m_pTexture = &m_pRenderer->createTexture(m_strTextureName);
}

CGUITexture_Impl::~CGUITexture_Impl()
{
    if (m_pTexture)
        m_pRenderer->destroyTexture(*m_pTexture);
}

bool CGUITexture_Impl::LoadFromFile(const char* szFilename)
{
    // Callers pass either one of MTA's own hardcoded "cgui\images\..." paths (relative to the MTA
    // folder, never the active skin's folder) or an already-resolved absolute path (script
    // resources); an empty resource group doesn't fall back to any working directory under CEGUI
    // 0.8.7 like it did under the old patched CEGUI 0.4, so it has to be picked explicitly here.
    const CEGUI::String strResourceGroup = SharedUtil::IsAbsolutePath(szFilename) ? "absolute" : "mta-images";

    // Try to load it
    try
    {
        m_pTexture->loadFromFile(szFilename, strResourceGroup);
    }
    catch (CEGUI::Exception)
    {
        return false;
    }
    return true;
}

void CGUITexture_Impl::LoadFromMemory(const void* pBuffer, unsigned int uiWidth, unsigned int uiHeight)
{
    m_pTexture->loadFromMemory(pBuffer, CEGUI::Sizef(static_cast<float>(uiWidth), static_cast<float>(uiHeight)), CEGUI::Texture::PF_RGBA);
}

void CGUITexture_Impl::Clear()
{
    // Destroy the previous texture and recreate it (empty), reusing the same name
    m_pRenderer->destroyTexture(*m_pTexture);
    m_pTexture = &m_pRenderer->createTexture(m_strTextureName);
}

CEGUI::Texture* CGUITexture_Impl::GetTexture()
{
    return m_pTexture;
}

void CGUITexture_Impl::SetTexture(CEGUI::Texture* pTexture)
{
    m_pTexture = pTexture;
}

LPDIRECT3DTEXTURE9 CGUITexture_Impl::GetD3DTexture()
{
    return reinterpret_cast<CEGUI::Direct3D9Texture*>(m_pTexture)->getDirect3D9Texture();
}

void CGUITexture_Impl::CreateTexture(unsigned int width, unsigned int height)
{
    // The old CEGUI 0.4 DirectX9Texture could turn itself into a render target in place; CEGUI
    // 0.8.7's renderer has no equivalent on Texture itself, so this just recreates a blank
    // texture of the requested size under the same name instead.
    m_pRenderer->destroyTexture(*m_pTexture);
    m_pTexture = &m_pRenderer->createTexture(m_strTextureName, CEGUI::Sizef(static_cast<float>(width), static_cast<float>(height)));
}
