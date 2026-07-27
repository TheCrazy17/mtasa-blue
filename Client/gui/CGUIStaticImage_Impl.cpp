/*****************************************************************************
 *
 *  PROJECT:     Multi Theft Auto v1.0
 *  LICENSE:     See LICENSE in the top level directory
 *  FILE:        gui/CGUIStaticImage_Impl.cpp
 *  PURPOSE:     Static image widget class
 *
 *  Multi Theft Auto is available from https://www.multitheftauto.com/
 *
 *****************************************************************************/

#include "StdInc.h"
#include <CEGUI/BasicImage.h>

#define CGUISTATICIMAGE_NAME "CGUI/StaticImage"

CGUIStaticImage_Impl::CGUIStaticImage_Impl(CGUI_Impl* pGUI, CGUIElement* pParent)
{
    // Initialize
    m_pImageManager = pGUI->GetImageSetManager();
    m_pImage = NULL;
    m_pGUI = pGUI;
    SetManager(pGUI);
    m_pTexture = NULL;
    m_bCreatedTexture = false;

    // Get an unique identifier for CEGUI
    char szUnique[CGUI_CHAR_SIZE];
    pGUI->GetUniqueName(szUnique);

    // Create the control and set default properties
    m_pWindow = pGUI->GetWindowManager()->createWindow(CGUISTATICIMAGE_NAME, szUnique);
    m_pWindow->setDestroyedByParent(false);
    m_pWindow->setArea(CEGUI::UDim(0.0f, 0.0f), CEGUI::UDim(0.0f, 0.0f), CEGUI::UDim(1.0f, 0.0f), CEGUI::UDim(1.0f, 0.0f));
    GetStaticImage()->setBackgroundEnabled(false);

    // Store the pointer to this CGUI element in the CEGUI element
    m_pWindow->setUserData(reinterpret_cast<void*>(this));

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

CGUIStaticImage_Impl::~CGUIStaticImage_Impl()
{
    // Clear the image
    Clear();

    DestroyElement();
}

bool CGUIStaticImage_Impl::LoadFromFile(const char* szFilename)
{
    // Load texture
    if (!m_pTexture)
    {
        m_pTexture = new CGUITexture_Impl(m_pGUI);
        m_bCreatedTexture = true;
    }

    if (!m_pTexture->LoadFromFile(szFilename))
        return false;

    // Load image
    return LoadFromTexture(m_pTexture);
}

bool CGUIStaticImage_Impl::LoadFromTexture(CGUITexture* pTexture)
{
    if (!m_strImageName.empty())
    {
        m_pImageManager->destroy(m_strImageName);
        m_strImageName.clear();
    }

    if (m_pTexture && pTexture != m_pTexture)
    {
        if (m_bCreatedTexture)
        {
            delete m_pTexture;
            m_pTexture = NULL;
            m_bCreatedTexture = false;
        }
    }

    m_pTexture = (CGUITexture_Impl*)pTexture;

    // Get CEGUI texture
    CEGUI::Texture* pCEGUITexture = m_pTexture->GetTexture();

    // Get an unique identifier for CEGUI for the image
    char szUnique[CGUI_CHAR_SIZE];
    m_pGUI->GetUniqueName(szUnique);
    while (m_pImageManager->isDefined(szUnique))
        m_pGUI->GetUniqueName(szUnique);
    m_strImageName = szUnique;

    // Define an image covering the whole texture and get its pointer
    CEGUI::BasicImage& image = static_cast<CEGUI::BasicImage&>(m_pImageManager->create("BasicImage", m_strImageName));
    image.setTexture(pCEGUITexture);
    image.setArea(CEGUI::Rectf(CEGUI::Vector2f(0.0f, 0.0f), pCEGUITexture->getSize()));
    m_pImage = &image;

    // Set the image just loaded as the image to be drawn for the widget
    GetStaticImage()->setImage(m_pImage);

    // Success
    return true;
}

void CGUIStaticImage_Impl::Clear()
{
    // Stop the control from using it
    GetStaticImage()->setImage(NULL);

    // Kill the image
    if (!m_strImageName.empty())
    {
        m_pImageManager->destroy(m_strImageName);
        m_strImageName.clear();
        if (m_bCreatedTexture)
        {
            delete m_pTexture;
            m_pTexture = NULL;
            m_bCreatedTexture = false;
        }
        m_pImage = NULL;
    }
}

bool CGUIStaticImage_Impl::GetNativeSize(CVector2D& vecSize)
{
    if (m_pTexture)
    {
        if (m_pTexture->GetTexture())
        {
            const CEGUI::Sizef& size = m_pTexture->GetTexture()->getSize();
            vecSize.fX = size.d_width;
            vecSize.fY = size.d_height;
            return true;
        }
    }
    return false;
}

void CGUIStaticImage_Impl::SetFrameEnabled(bool bFrameEnabled)
{
    GetStaticImage()->setFrameEnabled(bFrameEnabled);
}

bool CGUIStaticImage_Impl::IsFrameEnabled()
{
    return GetStaticImage()->isFrameEnabled();
}

CEGUI::Image* CGUIStaticImage_Impl::GetDirectImage()
{
    return const_cast<CEGUI::Image*>(GetStaticImage()->getImage());
}

void CGUIStaticImage_Impl::Render()
{
    return GetStaticImage()->render();
}
