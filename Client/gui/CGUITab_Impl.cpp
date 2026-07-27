/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto v1.0
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        gui/CGUITab_Impl.cpp
 *  PURPOSE:     Tab widget class
 *
 *  Multi Theft Auto is available from https://www.multitheftauto.com/
 *
 *****************************************************************************/

#include "StdInc.h"

namespace
{
    // TabControl::getButtonForTabContents() is protected in CEGUI 0.8.7 (it was public in 0.4),
    // so find our TabButton the same way TabControl does internally: by matching each button's
    // target content window against ours.
    CEGUI::TabButton* FindButtonForTabContents(CEGUI::TabControl* pControl, CEGUI::Window* pContentWindow)
    {
        const size_t childCount = pControl->getChildCount();
        for (size_t i = 0; i < childCount; ++i)
        {
            if (auto* pButton = dynamic_cast<CEGUI::TabButton*>(pControl->getChildAtIdx(i)))
            {
                if (pButton->getTargetWindow() == pContentWindow)
                    return pButton;
            }
        }
        return nullptr;
    }
}

CGUITab_Impl::CGUITab_Impl(CGUI_Impl* pGUI, CGUIElement_Impl* pParent, const char* szCaption)
{
    SetManager(pGUI);

    // Get an unique identifier for CEGUI
    char szUnique[CGUI_CHAR_SIZE];
    pGUI->GetUniqueName(szUnique);

    // Create the window and set default settings
    m_pWindow = pGUI->GetWindowManager()->createWindow("DefaultWindow", szUnique);
    m_pWindow->setDestroyedByParent(false);

    m_pWindow->setText(CGUI_Impl::GetUTFString(szCaption));

    // Store the pointer to this CGUI element in the CEGUI element
    m_pWindow->setUserData(reinterpret_cast<void*>(this));

    AddEvents();

    // If a parent is specified, add it to it's children list, if not, add it as a child to the pManager
    if (pParent)
    {
        SetParent(pParent);

        // Adjust the tab button (pParent should be a TabControl!)
        reinterpret_cast<CEGUI::TabControl*>(((CGUITabPanel_Impl*)pParent)->m_pWindow)->setTabHeight(CEGUI::UDim(0.0f, 24.0f));
    }
    else
    {
        pGUI->AddChild(this);
        SetParent(NULL);
    }
}

CGUITab_Impl::~CGUITab_Impl()
{
    CGUIElement_Impl*  pParent = static_cast<CGUIElement_Impl*>(m_pParent);
    CEGUI::TabControl* pControl = reinterpret_cast<CEGUI::TabControl*>(((CGUITabPanel_Impl*)pParent)->m_pWindow);
    pControl->removeTab(this->GetWindow()->getName());

    DestroyElement();
}

void CGUITab_Impl::SetCaption(const char* szCaption)
{
    m_pWindow->setText(CGUI_Impl::GetUTFString(szCaption));
}

void CGUITab_Impl::SetVisible(bool bVisible)
{
    CGUIElement_Impl*  pParent = static_cast<CGUIElement_Impl*>(m_pParent);
    CEGUI::TabControl* pControl = reinterpret_cast<CEGUI::TabControl*>(((CGUITabPanel_Impl*)pParent)->m_pWindow);
    if (auto* pButton = FindButtonForTabContents(pControl, m_pWindow))
        pButton->setVisible(bVisible);
    // requestChildWindowLayout() is gone (and its replacement, performChildWindowLayout(), isn't
    // public), so just invalidate the tab strip recursively to make it redo its layout.
    pControl->invalidate(true);
}

bool CGUITab_Impl::IsVisible()
{
    CGUIElement_Impl*  pParent = static_cast<CGUIElement_Impl*>(m_pParent);
    CEGUI::TabControl* pControl = reinterpret_cast<CEGUI::TabControl*>(((CGUITabPanel_Impl*)pParent)->m_pWindow);
    auto*               pButton = FindButtonForTabContents(pControl, m_pWindow);
    return pButton && pButton->isVisible();
}

void CGUITab_Impl::SetEnabled(bool bEnabled)
{
    CGUIElement_Impl*  pParent = static_cast<CGUIElement_Impl*>(m_pParent);
    CEGUI::TabControl* pControl = reinterpret_cast<CEGUI::TabControl*>(((CGUITabPanel_Impl*)pParent)->m_pWindow);
    if (auto* pButton = FindButtonForTabContents(pControl, m_pWindow))
        pButton->setEnabled(bEnabled);
}

bool CGUITab_Impl::IsEnabled()
{
    CGUIElement_Impl*  pParent = static_cast<CGUIElement_Impl*>(m_pParent);
    CEGUI::TabControl* pControl = reinterpret_cast<CEGUI::TabControl*>(((CGUITabPanel_Impl*)pParent)->m_pWindow);
    auto*               pButton = FindButtonForTabContents(pControl, m_pWindow);
    return pButton && !pButton->isDisabled();
}
