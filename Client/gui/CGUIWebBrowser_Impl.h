/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto v1.0
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        gui/CGUIWebBrowser_Impl.h
 *  PURPOSE:     WebBrowser CGUI class
 *
 *  Multi Theft Auto is available from https://www.multitheftauto.com/
 *
 *****************************************************************************/
#pragma once

#include <gui/CGUIWebBrowser.h>
#include "CGUITexture_Impl.h"
#include <CEGUI/WindowRendererSets/Core/StaticImage.h>
#include <CEGUI/BasicImage.h>

// Use StaticImage here as we'd have to add the same definition twice to the Falagard definition file otherwise
#define CGUIWEBBROWSER_NAME "CGUI/StaticImage"

class CGUITexture;
class CGUITexture_Impl;
class CGUI_Impl;
class CWebViewInterface;

class CGUIWebBrowser_Impl : public CGUIWebBrowser, public CGUIElement_Impl
{
public:
    CGUIWebBrowser_Impl(CGUI_Impl* pGUI, CGUIElement* pParent = nullptr);
    ~CGUIWebBrowser_Impl();
    void Clear();

    void LoadFromWebView(CWebViewInterface* pWebView);

    void SetFrameEnabled(bool bFrameEnabled);
    bool IsFrameEnabled();

    CEGUI::Image* GetDirectImage();
    void          Render();

    virtual eCGUIType GetType() override { return CGUI_WEBBROWSER; }

    bool HasInputFocus();

    virtual void SetSize(const CVector2D& vecSize, bool bRelative = false) override;

protected:
    bool Event_MouseButtonDown(const CEGUI::EventArgs& e);
    bool Event_MouseButtonUp(const CEGUI::EventArgs& e);
    bool Event_MouseDoubleClick(const CEGUI::EventArgs& e);
    bool Event_MouseWheel(const CEGUI::EventArgs& e);
    bool Event_MouseMove(const CEGUI::EventArgs& e);
    bool Event_Activated(const CEGUI::EventArgs& e);
    bool Event_Deactivated(const CEGUI::EventArgs& e);

private:
    // CEGUI 0.8.7 has no equivalent of the old subclassable DirectX9Texture that re-queried the
    // browser's D3D texture every frame via virtual calls, so we instead wrap whatever texture
    // the web view currently has (see RebindTexture()) and re-wrap it on resize, since that's the
    // one point in the existing flow where the CEF texture is known to get recreated.
    void RebindTexture();

    // "CGUI/StaticImage" is rendered by a Core/StaticImage window renderer attached to a plain
    // window in CEGUI 0.8.7, rather than being its own Window subclass like it was in 0.4.
    CEGUI::FalagardStaticImage* GetStaticImage() const { return static_cast<CEGUI::FalagardStaticImage*>(m_pWindow->getWindowRenderer()); }

    CGUI_Impl*           m_pGUI;
    CEGUI::Texture*      m_pTexture;
    CEGUI::BasicImage*   m_pImage;
    CEGUI::String        m_strImageName;

    CWebViewInterface* m_pWebView;

#define EXCLUDE_SET_SIZE  // WTF? TODO: Refactor this
#include "CGUIElement_Inc.h"
#undef EXCLUDE_SET_SIZE
};
