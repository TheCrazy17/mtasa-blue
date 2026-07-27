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
// Even linking CEGUI statically (CEGUI_STATIC), System.cpp's XML parser setup still calls the
// free functions below directly rather than going through DynamicModule, so they need to exist
// somewhere; this mirrors the equivalent file the stock RapidXML module ships.
#include "CEGUI/XMLParserModules/TinyXML2/XMLParser.h"

// System.cpp declares these as extern "C" when CEGUI_STATIC is defined (see its top), so the
// definitions have to match that linkage or the linker looks for a differently mangled name.
extern "C"
{

//----------------------------------------------------------------------------//
CEGUI::XMLParser* createParser(void)
{
    return CEGUI_NEW_AO CEGUI::TinyXML2Parser();
}

//----------------------------------------------------------------------------//
void destroyParser(CEGUI::XMLParser* parser)
{
    CEGUI_DELETE_AO parser;
}

//----------------------------------------------------------------------------//

}
