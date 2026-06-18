/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto v1.0
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        gui/CGUIScrollBar_Impl.cpp
 *  PURPOSE:     Scroll bar widget class
 *
 *  Multi Theft Auto is available from https://www.multitheftauto.com/
 *
 *****************************************************************************/

#include "StdInc.h"

#define CGUISCROLLBAR_HORIZONTAL_NAME "CGUI/HorizontalScrollbar"
#define CGUISCROLLBAR_VERTICAL_NAME   "CGUI/VerticalScrollbar"

CGUIScrollBar_Impl::CGUIScrollBar_Impl(CGUI_Impl* pGUI, bool bHorizontal, CGUIElement* pParent)
{
    SetManager(pGUI);

    // Get an unique identifier for CEGUI (gah, there's gotta be an another way)
    char szUnique[CGUI_CHAR_SIZE];
    pGUI->GetUniqueName(szUnique);

    // Create the window and set default settings
    m_pWindow = pGUI->GetWindowManager()->createWindow(bHorizontal ? CGUISCROLLBAR_HORIZONTAL_NAME : CGUISCROLLBAR_VERTICAL_NAME, szUnique);
    m_pWindow->setDestroyedByParent(false);

    // Store the pointer to this CGUI element in the CEGUI element
    m_pWindow->setUserData(reinterpret_cast<void*>(this));

    // Register our events
    m_pWindow->subscribeEvent(CEGUI::Scrollbar::EventScrollPositionChanged, CEGUI::Event::Subscriber(&CGUIScrollBar_Impl::Event_OnScroll, this));
    AddEvents();

    // If a parent is specified, add it to it's children list, if not, add it as a child to the pManager
    if (pParent)
    {
        SetParent(pParent);
    }
    else
    {
        pGUI->AddChild(this);
        SetParent(NULL);
    }
}

CGUIScrollBar_Impl::~CGUIScrollBar_Impl()
{
    DestroyElement();
}

void CGUIScrollBar_Impl::SetScrollPosition(float fPosition)
{
    reinterpret_cast<CEGUI::Scrollbar*>(m_pWindow)->setScrollPosition(fPosition);
}

float CGUIScrollBar_Impl::GetScrollPosition()
{
    return reinterpret_cast<CEGUI::Scrollbar*>(m_pWindow)->getScrollPosition();
}

void CGUIScrollBar_Impl::SetThumbDynamic(bool bDynamic)
{
    m_pWindow->setProperty("DynamicThumb", bDynamic ? "True" : "False");
}

bool CGUIScrollBar_Impl::GetThumbDynamic()
{
    return m_pWindow->getProperty("DynamicThumb") == "True";
}

void CGUIScrollBar_Impl::SetScrollBarThumbSize(float fSize)
{
    char szBuf[32];
    snprintf(szBuf, sizeof(szBuf), "%g", fSize);
    m_pWindow->setProperty("ThumbSize", szBuf);
}

float CGUIScrollBar_Impl::GetScrollBarThumbSize()
{
    return static_cast<float>(atof(m_pWindow->getProperty("ThumbSize").c_str()));
}

void CGUIScrollBar_Impl::SetScrollBarDocumentSize(float fSize)
{
    reinterpret_cast<CEGUI::Scrollbar*>(m_pWindow)->setDocumentSize(fSize);
}

float CGUIScrollBar_Impl::GetScrollBarDocumentSize()
{
    return reinterpret_cast<CEGUI::Scrollbar*>(m_pWindow)->getDocumentSize();
}

void CGUIScrollBar_Impl::SetScrollBarPageSize(float fSize)
{
    reinterpret_cast<CEGUI::Scrollbar*>(m_pWindow)->setPageSize(fSize);
}

float CGUIScrollBar_Impl::GetScrollBarPageSize()
{
    return reinterpret_cast<CEGUI::Scrollbar*>(m_pWindow)->getPageSize();
}

void CGUIScrollBar_Impl::SetOnScrollHandler(const GUI_CALLBACK& Callback)
{
    m_OnScroll = Callback;
}

bool CGUIScrollBar_Impl::Event_OnScroll(const CEGUI::EventArgs& e)
{
    if (m_OnScroll)
        m_OnScroll(reinterpret_cast<CGUIElement*>(this));
    return true;
}
