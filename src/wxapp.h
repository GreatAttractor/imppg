/*
ImPPG (Image Post-Processor) - common operations for astronomical stacks and other images
Copyright (C) 2016-2019 Filip Szczerek <ga.software@yahoo.com>

This file is part of ImPPG.

ImPPG is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

ImPPG is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with ImPPG.  If not, see <http://www.gnu.org/licenses/>.

File description:
    wxWidgets application class.
*/

#ifndef WXAPP_H_
#define WXAPP_H_

#include <fstream>
#include <wx/app.h>
#include <wx/fileconf.h>
#include <wx/intl.h>

class c_MyApp: public wxApp
{
    std::ofstream *m_LogStream;
    wxFileConfig *m_AppConfig;

    wxLanguage m_Language; ///< Language specified by user
    wxLocale m_Locale; ///< Locale to be used

    virtual bool OnInit();
    virtual int OnExit();

public:
    /// Returns current UI language
    wxLanguage GetLanguage() { return m_Language; }
};


#endif /* WXAPP_H_ */
