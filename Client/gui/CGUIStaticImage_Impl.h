/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto v1.0
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        gui/CGUIStaticImage_Impl.h
 *  PURPOSE:     Static image widget class
 *
 *  Multi Theft Auto is available from https://www.multitheftauto.com/
 *
 *****************************************************************************/

#pragma once

#include <gui/CGUIStaticImage.h>
#include "CGUITexture_Impl.h"
#include <CEGUI/WindowRendererSets/Core/StaticImage.h>

class CGUITexture;
class CGUITexture_Impl;

class CGUIStaticImage_Impl : public CGUIStaticImage, public CGUIElement_Impl, public CGUITabList
{
public:
    CGUIStaticImage_Impl(class CGUI_Impl* pGUI, CGUIElement* pParent = NULL);
    ~CGUIStaticImage_Impl();

    bool LoadFromFile(const char* szFilename);
    bool LoadFromTexture(CGUITexture* pTexture);
    bool GetNativeSize(CVector2D& vecSize);
    void Clear();

    void SetFrameEnabled(bool bFrameEnabled);
    bool IsFrameEnabled();

    CEGUI::Image* GetDirectImage();

    void Render();

    eCGUIType GetType() { return CGUI_STATICIMAGE; }

private:
    // "CGUI/StaticImage" is rendered by a Core/StaticImage window renderer attached to a plain
    // window in CEGUI 0.8.7, rather than being its own Window subclass like it was in 0.4.
    CEGUI::FalagardStaticImage* GetStaticImage() const { return static_cast<CEGUI::FalagardStaticImage*>(m_pWindow->getWindowRenderer()); }

    class CGUI_Impl*     m_pGUI;
    bool                 m_bCreatedTexture;
    CGUITexture_Impl*    m_pTexture;
    CEGUI::ImageManager* m_pImageManager;
    CEGUI::String        m_strImageName;
    const CEGUI::Image*  m_pImage;

#include "CGUIElement_Inc.h"
};
