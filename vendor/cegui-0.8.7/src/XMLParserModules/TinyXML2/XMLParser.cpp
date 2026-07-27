/***********************************************************************
    created:    27/7/2026
 *************************************************************************/
/***************************************************************************
 *   Copyright (C) 2004 - 2010 Paul D Turner & The CEGUI Development Team
 *
 *   Permission is hereby granted, free of charge, to any person obtaining
 *   a copy of this software and associated documentation files (the
 *   "Software"), to deal in the Software without restriction, including
 *   without limitation the rights to use, copy, modify, merge, publish,
 *   distribute, sublicense, and/or sell copies of the Software, and to
 *   permit persons to whom the Software is furnished to do so, subject to
 *   the following conditions:
 *
 *   The above copyright notice and this permission notice shall be
 *   included in all copies or substantial portions of the Software.
 *
 *   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 *   EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 *   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 *   IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 *   OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 *   ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 *   OTHER DEALINGS IN THE SOFTWARE.
 ***************************************************************************/
#include "tinyxml2.h"

#include "CEGUI/XMLParserModules/TinyXML2/XMLParser.h"
#include "CEGUI/ResourceProvider.h"
#include "CEGUI/System.h"
#include "CEGUI/XMLHandler.h"
#include "CEGUI/XMLAttributes.h"
#include "CEGUI/Logger.h"
#include "CEGUI/Exceptions.h"

// Start of CEGUI namespace section
namespace CEGUI
{

//----------------------------------------------------------------------------//
namespace
{
    void processElement(XMLHandler& handler, const tinyxml2::XMLElement* element)
    {
        XMLAttributes attrs;

        for (const tinyxml2::XMLAttribute* attr = element->FirstAttribute(); attr; attr = attr->Next())
        {
            attrs.add(reinterpret_cast<const encoded_char*>(attr->Name()),
                      reinterpret_cast<const encoded_char*>(attr->Value()));
        }

        handler.elementStart(reinterpret_cast<const encoded_char*>(element->Name()), attrs);

        for (const tinyxml2::XMLNode* child = element->FirstChild(); child; child = child->NextSibling())
        {
            if (const tinyxml2::XMLElement* childElement = child->ToElement())
                processElement(handler, childElement);
            else if (const tinyxml2::XMLText* childText = child->ToText())
            {
                if (childText->Value() != nullptr)
                    handler.text(reinterpret_cast<const encoded_char*>(childText->Value()));
            }
        }

        handler.elementEnd(reinterpret_cast<const encoded_char*>(element->Name()));
    }
}

//----------------------------------------------------------------------------//
TinyXML2Parser::TinyXML2Parser(void)
{
    d_identifierString = "CEGUI::TinyXML2Parser - "
                         "MTA's tinyxml2 based parser module for CEGUI";
}

//----------------------------------------------------------------------------//
TinyXML2Parser::~TinyXML2Parser(void)
{
}

//----------------------------------------------------------------------------//
void TinyXML2Parser::parseXML(XMLHandler& handler,
                              const RawDataContainer& source,
                              const String& /*schemaName*/)
{
    tinyxml2::XMLDocument doc;

    if (doc.Parse(reinterpret_cast<const char*>(source.getDataPtr()), source.getSize()) != tinyxml2::XML_SUCCESS)
        CEGUI_THROW(FileIOException("an error occurred while parsing the XML "
                                     "data - check it for potential errors!"));

    if (const tinyxml2::XMLElement* root = doc.RootElement())
        processElement(handler, root);
}

//----------------------------------------------------------------------------//
bool TinyXML2Parser::initialiseImpl(void)
{
    return true;
}

//----------------------------------------------------------------------------//
void TinyXML2Parser::cleanupImpl(void)
{
}

//----------------------------------------------------------------------------//

} // End of  CEGUI namespace section
