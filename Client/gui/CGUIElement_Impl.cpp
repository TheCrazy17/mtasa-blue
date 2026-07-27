/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto v1.0
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        gui/CGUIElement_Impl.cpp
 *  PURPOSE:     Element (widget) base class
 *
 *  Multi Theft Auto is available from https://www.multitheftauto.com/
 *
 *****************************************************************************/

#include "StdInc.h"
#include "CGUI_Impl.h"

// Define no-drawing zones, a.k.a. the inside borders in the FrameWindow of BlueLook in pixels
// If something is drawn inside of these areas, the theme border is drawn on top of it
#define CGUI_NODRAW_LEFT   9.0f
#define CGUI_NODRAW_RIGHT  9.0f
#define CGUI_NODRAW_TOP    9.0f
#define CGUI_NODRAW_BOTTOM 9.0f

CGUIElement_Impl::CGUIElement_Impl()
{
    m_pData = NULL;
    m_pWindow = NULL;
    m_pParent = NULL;
    m_pManager = NULL;
    m_redrawHandle = CGUI_Impl::kInvalidRedrawHandle;
}

void CGUIElement_Impl::SetManager(CGUI_Impl* pManager)
{
    if (m_pManager == pManager)
        return;

    if (m_pManager && m_redrawHandle != CGUI_Impl::kInvalidRedrawHandle)
    {
        m_pManager->ReleaseRedrawHandle(m_redrawHandle);
        m_redrawHandle = CGUI_Impl::kInvalidRedrawHandle;
    }

    m_pManager = pManager;

    if (m_pManager)
    {
        m_redrawHandle = m_pManager->RegisterRedrawHandle(this);
    }
}

void CGUIElement_Impl::UnregisterFromRedrawQueue()
{
    if (m_pManager && m_redrawHandle != CGUI_Impl::kInvalidRedrawHandle)
    {
        m_pManager->RemoveFromRedrawQueue(this);
    }
}

void CGUIElement_Impl::DestroyElement()
{
    UnregisterFromRedrawQueue();

    if (m_pWindow)
    {
        // Clear pointer back to this
        m_pWindow->setUserData(NULL);

        if (m_pManager)
        {
            // Destroy the control
            m_pManager->GetWindowManager()->destroyWindow(m_pWindow);
        }
        m_pWindow = NULL;
    }

    // Destroy the properties list
    EmptyProperties();

    if (m_pManager && m_redrawHandle != CGUI_Impl::kInvalidRedrawHandle)
    {
        m_pManager->ReleaseRedrawHandle(m_redrawHandle);
    }

    m_redrawHandle = CGUI_Impl::kInvalidRedrawHandle;
    m_pParent = NULL;
    m_pData = NULL;
    m_pManager = NULL;
}

void CGUIElement_Impl::SetVisible(bool bVisible)
{
    m_pWindow->setVisible(bVisible);
}

bool CGUIElement_Impl::IsVisible()
{
    return m_pWindow->isVisible();
}

void CGUIElement_Impl::SetEnabled(bool bEnabled)
{
    m_pWindow->setEnabled(bEnabled);
    // m_pWindow->setZOrderingEnabled ( bEnabled );
}

bool CGUIElement_Impl::IsEnabled()
{
    return !m_pWindow->isDisabled();
}

void CGUIElement_Impl::SetZOrderingEnabled(bool bZOrderingEnabled)
{
    m_pWindow->setZOrderingEnabled(bZOrderingEnabled);
}

bool CGUIElement_Impl::IsZOrderingEnabled()
{
    return m_pWindow->isZOrderingEnabled();
}

void CGUIElement_Impl::BringToFront()
{
    m_pWindow->moveToFront();
}

void CGUIElement_Impl::MoveToBack()
{
    m_pWindow->moveToBack();
}

void CGUIElement_Impl::SetPosition(const CVector2D& Position, bool bRelative)
{
    m_pWindow->setPosition(CEGUI::UVector2(CGUI_Impl::MakeUDim(Position.fX, bRelative), CGUI_Impl::MakeUDim(Position.fY, bRelative)));

    CorrectEdges();
}

CVector2D CGUIElement_Impl::GetPosition(bool bRelative)
{
    const CEGUI::UVector2& Pos = m_pWindow->getPosition();
    const CEGUI::Sizef     Base = m_pWindow->getParentPixelSize();

    return CVector2D(CGUI_Impl::ResolveUDim(Pos.d_x, Base.d_width, bRelative), CGUI_Impl::ResolveUDim(Pos.d_y, Base.d_height, bRelative));
}

void CGUIElement_Impl::GetPosition(CVector2D& vecPosition, bool bRelative)
{
    vecPosition = GetPosition(bRelative);
}

void CGUIElement_Impl::SetWidth(float fX, bool bRelative)
{
    m_pWindow->setWidth(CGUI_Impl::MakeUDim(fX, bRelative));
}

void CGUIElement_Impl::SetHeight(float fY, bool bRelative)
{
    m_pWindow->setHeight(CGUI_Impl::MakeUDim(fY, bRelative));
}

void CGUIElement_Impl::SetSize(const CVector2D& vecSize, bool bRelative)
{
    m_pWindow->setSize(CEGUI::USize(CGUI_Impl::MakeUDim(vecSize.fX, bRelative), CGUI_Impl::MakeUDim(vecSize.fY, bRelative)));

    CorrectEdges();
}

CVector2D CGUIElement_Impl::GetSize(bool bRelative)
{
    const CEGUI::USize& Size = m_pWindow->getSize();
    const CEGUI::Sizef  Base = m_pWindow->getParentPixelSize();

    return CVector2D(CGUI_Impl::ResolveUDim(Size.d_width, Base.d_width, bRelative), CGUI_Impl::ResolveUDim(Size.d_height, Base.d_height, bRelative));
}

void CGUIElement_Impl::GetSize(CVector2D& vecSize, bool bRelative)
{
    vecSize = GetSize(bRelative);
}

void CGUIElement_Impl::AutoSize(const char* Text, float fPaddingX, float fPaddingY)
{
    const CEGUI::Font* pFont = m_pWindow->getFont();
    m_pWindow->setSize(CEGUI::USize(CEGUI::UDim(0.0f, pFont->getTextExtent(CGUI_Impl::GetUTFString(Text ? Text : GetText())) + fPaddingX),
                                    CEGUI::UDim(0.0f, pFont->getFontHeight() + fPaddingY)));  // Add hack factor to height to allow for long characters such as 'g' or 'j'
}

void CGUIElement_Impl::SetMinimumSize(const CVector2D& vecSize)
{
    m_pWindow->setMinSize(CEGUI::USize(CEGUI::UDim(0.0f, vecSize.fX), CEGUI::UDim(0.0f, vecSize.fY)));
}

CVector2D CGUIElement_Impl::GetMinimumSize()
{
    const CEGUI::USize& TempSize = m_pWindow->getMinSize();
    return CVector2D(TempSize.d_width.d_offset, TempSize.d_height.d_offset);
}

void CGUIElement_Impl::GetMinimumSize(CVector2D& vecSize)
{
    vecSize = GetMinimumSize();
}

void CGUIElement_Impl::SetMaximumSize(const CVector2D& vecSize)
{
    m_pWindow->setMaxSize(CEGUI::USize(CEGUI::UDim(0.0f, vecSize.fX), CEGUI::UDim(0.0f, vecSize.fY)));
}

CVector2D CGUIElement_Impl::GetMaximumSize()
{
    const CEGUI::USize& TempSize = m_pWindow->getMaxSize();
    return CVector2D(TempSize.d_width.d_offset, TempSize.d_height.d_offset);
}

void CGUIElement_Impl::GetMaximumSize(CVector2D& vecSize)
{
    // Note: kept consistent with the pre-existing behaviour here, which reads the window's
    // current size rather than its maximum size.
    const CEGUI::Sizef& Temp = m_pWindow->getPixelSize();
    vecSize.fX = Temp.d_width;
    vecSize.fY = Temp.d_height;
}

void CGUIElement_Impl::SetText(const char* szText)
{
    m_pWindow->setText(CGUI_Impl::GetUTFString(szText));
}

std::string CGUIElement_Impl::GetText()
{
    return CGUI_Impl::GetUTFString(m_pWindow->getText().c_str()).c_str();
}

void CGUIElement_Impl::SetAlpha(float fAlpha)
{
    m_pWindow->setAlpha(fAlpha);
}

float CGUIElement_Impl::GetAlpha()
{
    return m_pWindow->getAlpha();
}

float CGUIElement_Impl::GetEffectiveAlpha()
{
    return m_pWindow->getEffectiveAlpha();
}

void CGUIElement_Impl::SetInheritsAlpha(bool bInheritsAlpha)
{
    m_pWindow->setInheritsAlpha(bInheritsAlpha);
}

bool CGUIElement_Impl::GetInheritsAlpha()
{
    return m_pWindow->inheritsAlpha();
}

void CGUIElement_Impl::Activate()
{
    m_pWindow->activate();
}

void CGUIElement_Impl::Deactivate()
{
    m_pWindow->deactivate();
}

bool CGUIElement_Impl::IsActive()
{
    return m_pWindow->isActive();
}

void CGUIElement_Impl::SetAlwaysOnTop(bool bAlwaysOnTop)
{
    m_pWindow->setAlwaysOnTop(bAlwaysOnTop);
}

bool CGUIElement_Impl::IsAlwaysOnTop()
{
    return m_pWindow->isAlwaysOnTop();
}

CRect2D CGUIElement_Impl::AbsoluteToRelative(const CRect2D& Rect)
{
    // Note: relative to this window's own pixel size, not its parent's (matches the old CEGUI
    // absoluteToRelative(Rect) semantics, which is a distinct concept from relative positioning).
    const CEGUI::Sizef& Base = m_pWindow->getPixelSize();
    return CRect2D(Rect.fX1 / Base.d_width, Rect.fY1 / Base.d_height, Rect.fX2 / Base.d_width, Rect.fY2 / Base.d_height);
}

CVector2D CGUIElement_Impl::AbsoluteToRelative(const CVector2D& Vector)
{
    const CEGUI::Sizef& Base = m_pWindow->getPixelSize();
    return CVector2D(Vector.fX / Base.d_width, Vector.fY / Base.d_height);
}

CRect2D CGUIElement_Impl::RelativeToAbsolute(const CRect2D& Rect)
{
    const CEGUI::Sizef& Base = m_pWindow->getPixelSize();
    return CRect2D(Rect.fX1 * Base.d_width, Rect.fY1 * Base.d_height, Rect.fX2 * Base.d_width, Rect.fY2 * Base.d_height);
}

CVector2D CGUIElement_Impl::RelativeToAbsolute(const CVector2D& Vector)
{
    const CEGUI::Sizef& Base = m_pWindow->getPixelSize();
    return CVector2D(Vector.fX * Base.d_width, Vector.fY * Base.d_height);
}

void CGUIElement_Impl::SetParent(CGUIElement* pParent)
{
    // Disable z-sorting if the label has a parent
    if (GetType() == CGUI_LABEL)
        m_pWindow->setZOrderingEnabled(pParent == NULL);

    if (pParent)
    {
        CGUIElement_Impl* pElement = dynamic_cast<CGUIElement_Impl*>(pParent);
        if (pElement)
            pElement->m_pWindow->addChild(m_pWindow);
    }
    m_pParent = pParent;
}

CGUIElement* CGUIElement_Impl::GetParent()
{
    // Validate
    if (m_pParent && m_pWindow && !m_pWindow->getParent())
        return NULL;

    return m_pParent;
}

void CGUIElement_Impl::CorrectEdges()
{
    const CEGUI::Sizef  parentBase = m_pWindow->getParentPixelSize();
    const CEGUI::UVector2& pos = m_pWindow->getPosition();
    CEGUI::Vector2f     currentPoint(CGUI_Impl::ResolveUDim(pos.d_x, parentBase.d_width, false), CGUI_Impl::ResolveUDim(pos.d_y, parentBase.d_height, false));
    CEGUI::Sizef        currentSize = m_pWindow->getPixelSize();
    // Label turns out to be buggy
    if (m_pWindow->getType() == "CGUI/StaticText")
        return;

    if (m_pWindow->getParent()->getType() == "CGUI/FrameWindow")
    {
        CEGUI::Sizef parentSize = m_pWindow->getParent()->getPixelSize();
        if (currentPoint.d_x < CGUI_NODRAW_LEFT)
            currentPoint.d_x += CGUI_NODRAW_LEFT - currentPoint.d_x;
        if (currentPoint.d_y < CGUI_NODRAW_TOP)
            currentPoint.d_y += CGUI_NODRAW_TOP - currentPoint.d_x;
        if ((currentSize.d_height + currentPoint.d_y) > (parentSize.d_height - CGUI_NODRAW_BOTTOM))
            currentSize.d_height -= (currentSize.d_height + currentPoint.d_y) - (parentSize.d_height - CGUI_NODRAW_BOTTOM);
        if ((currentSize.d_width + currentPoint.d_x) > (parentSize.d_width - CGUI_NODRAW_RIGHT))
            currentSize.d_width -= (currentSize.d_width + currentPoint.d_x) - (parentSize.d_width - CGUI_NODRAW_RIGHT);
        m_pWindow->setPosition(CEGUI::UVector2(CEGUI::UDim(0.0f, currentPoint.d_x), CEGUI::UDim(0.0f, currentPoint.d_y)));
        m_pWindow->setSize(CEGUI::USize(CEGUI::UDim(0.0f, currentSize.d_width), CEGUI::UDim(0.0f, currentSize.d_height)));
    }
}

bool CGUIElement_Impl::SetFont(const char* szFontName)
{
    if (szFontName != nullptr && *szFontName != '\0')
    {
        if (!CEGUI::FontManager::getSingleton().isDefined(CEGUI::String(szFontName)))
            return false;
    }

    try
    {
        m_pWindow->setFont(CEGUI::String(szFontName));
        return true;
    }
    catch (CEGUI::Exception e)
    {
        return false;
    }
}

std::string CGUIElement_Impl::GetFont()
{
    try
    {
        const CEGUI::Font* pFont = m_pWindow->getFont();
        if (pFont)
        {
            // Return the contname. std::string will copy it.
            CEGUI::String strFontName = pFont->getName();
            return strFontName.c_str();
        }
    }
    catch (CEGUI::Exception e)
    {
    }

    return "";
}

void CGUIElement_Impl::SetProperty(const char* szProperty, const char* szValue)
{
    try
    {
        m_pWindow->setProperty(CGUI_Impl::GetUTFString(szProperty), CGUI_Impl::GetUTFString(szValue));
    }
    catch (CEGUI::Exception e)
    {
    }
}

std::string CGUIElement_Impl::GetProperty(const char* szProperty)
{
    CEGUI::String strValue;
    try
    {
        // Return the string. std::string will copy it
        strValue = CGUI_Impl::GetUTFString(m_pWindow->getProperty(CGUI_Impl::GetUTFString(szProperty)).c_str());
    }
    catch (CEGUI::Exception e)
    {
    }

    return strValue.c_str();
}

void CGUIElement_Impl::FillProperties()
{
    CEGUI::PropertySet::PropertyIterator itPropertySet = m_pWindow->getPropertyIterator();
    while (!itPropertySet.isAtEnd())
    {
        CEGUI::String strKey = itPropertySet.getCurrentKey();
        CEGUI::String strValue = m_pWindow->getProperty(strKey);

        CGUIProperty* pProperty = new CGUIProperty;
        pProperty->strKey = strKey.c_str();
        pProperty->strValue = strValue.c_str();

        m_Properties.push_back(pProperty);
        itPropertySet++;
    }
}

void CGUIElement_Impl::EmptyProperties()
{
    if (!m_Properties.empty())
    {
        CGUIPropertyIter iter = m_Properties.begin();
        CGUIPropertyIter iterEnd = m_Properties.end();
        for (; iter != iterEnd; iter++)
        {
            if (*iter)
            {
                delete (*iter);
            }
        }
    }
}

CGUIPropertyIter CGUIElement_Impl::GetPropertiesBegin()
{
    try
    {
        // Fill the properties list, if it's still empty (on first call)
        if (m_Properties.empty())
            FillProperties();

        // Return the list begin iterator
        return m_Properties.begin();
    }
    catch (CEGUI::Exception e)
    {
        return *(CGUIPropertyIter*)NULL;
    }
}

CGUIPropertyIter CGUIElement_Impl::GetPropertiesEnd()
{
    try
    {
        // Fill the properties list, if it's still empty (on first call)
        if (m_Properties.empty())
            FillProperties();

        // Return the list begin iterator
        return m_Properties.end();
    }
    catch (CEGUI::Exception e)
    {
        return *(CGUIPropertyIter*)NULL;
    }
}

void CGUIElement_Impl::SetMovedHandler(GUI_CALLBACK Callback)
{
    m_OnMoved = Callback;
}

void CGUIElement_Impl::SetSizedHandler(GUI_CALLBACK Callback)
{
    m_OnSized = Callback;
}

void CGUIElement_Impl::SetClickHandler(GUI_CALLBACK Callback)
{
    m_OnClick = Callback;
}

void CGUIElement_Impl::SetClickHandler(const GUI_CALLBACK_MOUSE& Callback)
{
    m_OnClickWithArgs = Callback;
}

void CGUIElement_Impl::SetDoubleClickHandler(GUI_CALLBACK Callback)
{
    m_OnDoubleClick = Callback;
}

void CGUIElement_Impl::SetMouseEnterHandler(GUI_CALLBACK Callback)
{
    m_OnMouseEnter = Callback;
}

void CGUIElement_Impl::SetMouseLeaveHandler(GUI_CALLBACK Callback)
{
    m_OnMouseLeave = Callback;
}

void CGUIElement_Impl::SetMouseButtonDownHandler(GUI_CALLBACK Callback)
{
    m_OnMouseDown = Callback;
}

void CGUIElement_Impl::SetActivateHandler(GUI_CALLBACK Callback)
{
    m_OnActivate = Callback;
}

void CGUIElement_Impl::SetDeactivateHandler(GUI_CALLBACK Callback)
{
    m_OnDeactivate = Callback;
}

void CGUIElement_Impl::SetKeyDownHandler(GUI_CALLBACK Callback)
{
    m_OnKeyDown = Callback;
}

void CGUIElement_Impl::SetEnterKeyHandler(GUI_CALLBACK Callback)
{
    m_OnEnter = Callback;
}

void CGUIElement_Impl::SetKeyDownHandler(const GUI_CALLBACK_KEY& Callback)
{
    m_OnKeyDownWithArgs = Callback;
}

void CGUIElement_Impl::AddEvents()
{
    // Note: Mouse Click, Double Click, Enter, Leave and ButtonDown are handled by global events in CGUI_Impl
    // Register our events
    m_pWindow->subscribeEvent(CEGUI::Window::EventMoved, CEGUI::Event::Subscriber(&CGUIElement_Impl::Event_OnMoved, this));
    m_pWindow->subscribeEvent(CEGUI::Window::EventSized, CEGUI::Event::Subscriber(&CGUIElement_Impl::Event_OnSized, this));
    m_pWindow->subscribeEvent(CEGUI::Window::EventActivated, CEGUI::Event::Subscriber(&CGUIElement_Impl::Event_OnActivated, this));
    m_pWindow->subscribeEvent(CEGUI::Window::EventDeactivated, CEGUI::Event::Subscriber(&CGUIElement_Impl::Event_OnDeactivated, this));
    m_pWindow->subscribeEvent(CEGUI::Window::EventKeyDown, CEGUI::Event::Subscriber(&CGUIElement_Impl::Event_OnKeyDown, this));
}

bool CGUIElement_Impl::Event_OnMoved(const CEGUI::EventArgs& e)
{
    if (m_OnMoved)
        m_OnMoved(reinterpret_cast<CGUIElement*>(this));
    return true;
}

bool CGUIElement_Impl::Event_OnSized(const CEGUI::EventArgs& e)
{
    if (m_OnSized)
        m_OnSized(reinterpret_cast<CGUIElement*>(this));
    return true;
}

bool CGUIElement_Impl::Event_OnClick(const CEGUI::EventArgs& eBase)
{
    const CEGUI::MouseEventArgs& e = reinterpret_cast<const CEGUI::MouseEventArgs&>(eBase);
    CGUIElement*                 pElement = reinterpret_cast<CGUIElement*>(this);

    if (m_OnClick)
        m_OnClick(pElement);

    if (m_OnClickWithArgs)
    {
        CGUIMouseEventArgs NewArgs;

        // copy the variables
        NewArgs.button = static_cast<CGUIMouse::MouseButton>(e.button);
        NewArgs.moveDelta = CVector2D(e.moveDelta.d_x, e.moveDelta.d_y);
        NewArgs.position = CGUIPosition(e.position.d_x, e.position.d_y);
        NewArgs.sysKeys = e.sysKeys;
        NewArgs.wheelChange = e.wheelChange;
        NewArgs.pWindow = pElement;

        m_OnClickWithArgs(NewArgs);
    }

    return true;
}

bool CGUIElement_Impl::Event_OnDoubleClick()
{
    if (m_OnDoubleClick)
        m_OnDoubleClick(reinterpret_cast<CGUIElement*>(this));
    return true;
}

bool CGUIElement_Impl::Event_OnMouseEnter()
{
    if (m_OnMouseEnter)
        m_OnMouseEnter(reinterpret_cast<CGUIElement*>(this));
    return true;
}

bool CGUIElement_Impl::Event_OnMouseLeave()
{
    if (m_OnMouseLeave)
        m_OnMouseLeave(reinterpret_cast<CGUIElement*>(this));
    return true;
}

bool CGUIElement_Impl::Event_OnMouseButtonDown()
{
    if (m_OnMouseDown)
        m_OnMouseDown(reinterpret_cast<CGUIElement*>(this));
    return true;
}

bool CGUIElement_Impl::Event_OnActivated(const CEGUI::EventArgs& e)
{
    if (m_OnActivate)
        m_OnActivate(reinterpret_cast<CGUIElement*>(this));
    return true;
}

bool CGUIElement_Impl::Event_OnDeactivated(const CEGUI::EventArgs& e)
{
    if (m_OnDeactivate)
        m_OnDeactivate(reinterpret_cast<CGUIElement*>(this));
    return true;
}

bool CGUIElement_Impl::Event_OnKeyDown(const CEGUI::EventArgs& e)
{
    const CEGUI::KeyEventArgs& Args = reinterpret_cast<const CEGUI::KeyEventArgs&>(e);
    CGUIElement*               pCGUIElement = reinterpret_cast<CGUIElement*>(this);

    if (m_OnKeyDown)
    {
        m_OnKeyDown(pCGUIElement);
    }

    if (m_OnKeyDownWithArgs)
    {
        CGUIKeyEventArgs NewArgs;

        // copy the variables
        NewArgs.codepoint = Args.codepoint;
        NewArgs.scancode = (CGUIKeys::Scan)Args.scancode;
        NewArgs.sysKeys = Args.sysKeys;

        // get the CGUIElement
        CGUIElement* pElement = reinterpret_cast<CGUIElement*>((Args.window)->getUserData());
        NewArgs.pWindow = pElement;

        m_OnKeyDownWithArgs(NewArgs);
    }

    if (m_OnEnter)
    {
        switch (Args.scancode)
        {
            // Return key
            case CEGUI::Key::NumpadEnter:
            case CEGUI::Key::Return:
            {
                // Fire the event
                m_OnEnter(pCGUIElement);
                break;
            }
        }
    }

    return true;
}

inline void CGUIElement_Impl::ForceRedraw()
{
    m_pWindow->invalidate();
}
