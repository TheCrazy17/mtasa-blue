/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto v1.0
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        gui/CGUIWebBrowser_Impl.cpp
 *  PURPOSE:     WebBrowser widget class
 *
 *  Multi Theft Auto is available from https://www.multitheftauto.com/
 *
 *****************************************************************************/
#include "StdInc.h"
#include <core/CWebViewInterface.h>
#include <CEGUI/RendererModules/Direct3D9/Renderer.h>

CGUIWebBrowser_Impl::CGUIWebBrowser_Impl(CGUI_Impl* pGUI, CGUIElement* pParent)
{
    // Initialize
    m_pTexture = nullptr;
    m_pImage = nullptr;
    m_pGUI = pGUI;
    SetManager(pGUI);
    m_pWebView = nullptr;

    // Get an unique identifier for CEGUI
    char szUnique[CGUI_CHAR_SIZE];
    pGUI->GetUniqueName(szUnique);

    // Create the control and set default properties
    m_pWindow = pGUI->GetWindowManager()->createWindow(CGUIWEBBROWSER_NAME, szUnique);
    m_pWindow->setDestroyedByParent(false);
    m_pWindow->setArea(CEGUI::UDim(0.0f, 0.0f), CEGUI::UDim(0.0f, 0.0f), CEGUI::UDim(1.0f, 0.0f), CEGUI::UDim(1.0f, 0.0f));
    GetStaticImage()->setBackgroundEnabled(false);

    // Store the pointer to this CGUI element in the CEGUI element
    m_pWindow->setUserData(reinterpret_cast<void*>(this));

    AddEvents();

    // Apply browser events
    m_pWindow->subscribeEvent(CEGUI::Window::EventMouseButtonDown, CEGUI::Event::Subscriber(&CGUIWebBrowser_Impl::Event_MouseButtonDown, this));
    m_pWindow->subscribeEvent(CEGUI::Window::EventMouseButtonUp, CEGUI::Event::Subscriber(&CGUIWebBrowser_Impl::Event_MouseButtonUp, this));
    m_pWindow->subscribeEvent(CEGUI::Window::EventMouseDoubleClick, CEGUI::Event::Subscriber(&CGUIWebBrowser_Impl::Event_MouseDoubleClick, this));
    m_pWindow->subscribeEvent(CEGUI::Window::EventMouseMove, CEGUI::Event::Subscriber(&CGUIWebBrowser_Impl::Event_MouseMove, this));
    m_pWindow->subscribeEvent(CEGUI::Window::EventMouseWheel, CEGUI::Event::Subscriber(&CGUIWebBrowser_Impl::Event_MouseWheel, this));
    m_pWindow->subscribeEvent(CEGUI::Window::EventActivated, CEGUI::Event::Subscriber(&CGUIWebBrowser_Impl::Event_Activated, this));
    m_pWindow->subscribeEvent(CEGUI::Window::EventDeactivated, CEGUI::Event::Subscriber(&CGUIWebBrowser_Impl::Event_Deactivated, this));

    // If a parent is specified, add it to it's children list, if not, add it as a child to the pManager
    if (pParent)
    {
        SetParent(pParent);
    }
    else
    {
        pGUI->AddChild(this);
        SetParent(nullptr);
    }
}

CGUIWebBrowser_Impl::~CGUIWebBrowser_Impl()
{
    Clear();

    DestroyElement();
}

void CGUIWebBrowser_Impl::Clear()
{
    // Stop the control from using it
    GetStaticImage()->setImage(nullptr);

    if (m_pImage)
    {
        m_pGUI->GetImageSetManager()->destroy(m_strImageName);
        m_strImageName.clear();
        m_pImage = nullptr;
    }

    if (m_pTexture)
    {
        m_pGUI->GetRenderer()->destroyTexture(*m_pTexture);
        m_pTexture = nullptr;
    }
}

void CGUIWebBrowser_Impl::RebindTexture()
{
    if (!m_pWebView)
        return;

    if (m_pTexture)
        m_pGUI->GetRenderer()->destroyTexture(*m_pTexture);

    // Wrap the web view's existing D3D texture directly (name has to be unique per (re)bind,
    // since the old one was just destroyed above but CEGUI doesn't immediately free the name).
    char szUnique[CGUI_CHAR_SIZE];
    m_pGUI->GetUniqueName(szUnique);
    auto* pRenderer = static_cast<CEGUI::Direct3D9Renderer*>(m_pGUI->GetRenderer());
    m_pTexture = &pRenderer->createTexture(szUnique, m_pWebView->GetTexture());

    if (!m_pImage)
    {
        m_pGUI->GetUniqueName(szUnique);
        m_strImageName = szUnique;
        m_pImage = static_cast<CEGUI::BasicImage*>(&m_pGUI->GetImageSetManager()->create("BasicImage", m_strImageName));
    }

    m_pImage->setTexture(m_pTexture);
    m_pImage->setArea(CEGUI::Rectf(CEGUI::Vector2f(0.0f, 0.0f), m_pTexture->getSize()));

    GetStaticImage()->setImage(m_pImage);
}

void CGUIWebBrowser_Impl::LoadFromWebView(CWebViewInterface* pWebView)
{
    m_pWebView = pWebView;
    RebindTexture();
}

void CGUIWebBrowser_Impl::SetFrameEnabled(bool bFrameEnabled)
{
    GetStaticImage()->setFrameEnabled(bFrameEnabled);
}

bool CGUIWebBrowser_Impl::IsFrameEnabled()
{
    return GetStaticImage()->isFrameEnabled();
}

CEGUI::Image* CGUIWebBrowser_Impl::GetDirectImage()
{
    return m_pImage;
}

void CGUIWebBrowser_Impl::Render()
{
    GetStaticImage()->render();
}

bool CGUIWebBrowser_Impl::HasInputFocus()
{
    return m_pWebView->HasInputFocus();
}

void CGUIWebBrowser_Impl::SetSize(const CVector2D& vecSize, bool bRelative)
{
    // Call base class function
    CGUIElement_Impl::SetSize(vecSize, bRelative);
    auto absSize = CGUIElement_Impl::GetSize(false);

    // Resize underlying web view as well
    if (m_pWebView)
    {
        m_pWebView->Resize(absSize);

        // D3D render targets can't be resized in place, so CEF hands back a new texture whenever
        // the view is resized; re-wrap whatever texture it has now rather than just adjusting the
        // old image's source area.
        RebindTexture();
    }
}

bool CGUIWebBrowser_Impl::Event_MouseButtonDown(const CEGUI::EventArgs& e)
{
    const CEGUI::MouseEventArgs& args = reinterpret_cast<const CEGUI::MouseEventArgs&>(e);

    if (args.button == CEGUI::MouseButton::LeftButton)
        m_pWebView->InjectMouseDown(eWebBrowserMouseButton::BROWSER_MOUSEBUTTON_LEFT, 1);
    else if (args.button == CEGUI::MouseButton::MiddleButton)
        m_pWebView->InjectMouseDown(eWebBrowserMouseButton::BROWSER_MOUSEBUTTON_MIDDLE, 1);
    else if (args.button == CEGUI::MouseButton::RightButton)
        m_pWebView->InjectMouseDown(eWebBrowserMouseButton::BROWSER_MOUSEBUTTON_RIGHT, 1);

    return true;
}

bool CGUIWebBrowser_Impl::Event_MouseButtonUp(const CEGUI::EventArgs& e)
{
    const CEGUI::MouseEventArgs& args = reinterpret_cast<const CEGUI::MouseEventArgs&>(e);

    if (args.button == CEGUI::MouseButton::LeftButton)
        m_pWebView->InjectMouseUp(eWebBrowserMouseButton::BROWSER_MOUSEBUTTON_LEFT);
    else if (args.button == CEGUI::MouseButton::MiddleButton)
        m_pWebView->InjectMouseUp(eWebBrowserMouseButton::BROWSER_MOUSEBUTTON_MIDDLE);
    else if (args.button == CEGUI::MouseButton::RightButton)
        m_pWebView->InjectMouseUp(eWebBrowserMouseButton::BROWSER_MOUSEBUTTON_RIGHT);

    return true;
}

bool CGUIWebBrowser_Impl::Event_MouseDoubleClick(const CEGUI::EventArgs& e)
{
    const CEGUI::MouseEventArgs& args = reinterpret_cast<const CEGUI::MouseEventArgs&>(e);

    if (args.button == CEGUI::MouseButton::LeftButton)
        m_pWebView->InjectMouseDown(eWebBrowserMouseButton::BROWSER_MOUSEBUTTON_LEFT, 2);
    else if (args.button == CEGUI::MouseButton::MiddleButton)
        m_pWebView->InjectMouseDown(eWebBrowserMouseButton::BROWSER_MOUSEBUTTON_MIDDLE, 2);
    else if (args.button == CEGUI::MouseButton::RightButton)
        m_pWebView->InjectMouseDown(eWebBrowserMouseButton::BROWSER_MOUSEBUTTON_RIGHT, 2);

    return true;
}

bool CGUIWebBrowser_Impl::Event_MouseMove(const CEGUI::EventArgs& e)
{
    const CEGUI::MouseEventArgs& args = reinterpret_cast<const CEGUI::MouseEventArgs&>(e);

    const CEGUI::Vector2f localPos = CEGUI::CoordConverter::screenToWindow(*m_pWindow, args.position);
    m_pWebView->InjectMouseMove(static_cast<int>(localPos.d_x), static_cast<int>(localPos.d_y));
    return true;
}

bool CGUIWebBrowser_Impl::Event_MouseWheel(const CEGUI::EventArgs& e)
{
    const CEGUI::MouseEventArgs& args = reinterpret_cast<const CEGUI::MouseEventArgs&>(e);

    m_pWebView->InjectMouseWheel((int)(args.wheelChange * 40), 0);
    return true;
}

bool CGUIWebBrowser_Impl::Event_Activated(const CEGUI::EventArgs& e)
{
    m_pWebView->Focus(true);
    return true;
}

bool CGUIWebBrowser_Impl::Event_Deactivated(const CEGUI::EventArgs& e)
{
    m_pWebView->Focus(false);
    return true;
}
