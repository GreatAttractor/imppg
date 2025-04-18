/*
ImPPG (Image Post-Processor) - common operations for astronomical stacks and other images
Copyright (C) 2016-2025 Filip Szczerek <ga.software@yahoo.com>

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
    wxWidgets application implementation.
*/

#include <wx/log.h>
#include <wx/tooltip.h>
#include <wx/filename.h>
#include <wx/msgdlg.h>
#ifdef __WXMSW__
#include <wx/sysopt.h>
#endif

#include "common/imppg_assert.h"
#include "wxapp.h"
#include "appconfig.h"
#include "cursors.h"
#include "logging.h"
#include "main_window.h"
#if USE_FREEIMAGE
#include "FreeImage.h" // on MSW it has to be the last include (to make sure no wxW header follows it)
// On macOS FreeImage.h from Homebrew defines _WINDOWS_ (sic!) which affects negatively wx and who knows what else.
#ifdef __APPLE__
   #undef _WINDOWS_
#endif
#endif

bool c_MyApp::OnInit()
{
#if USE_FREEIMAGE
    FreeImage_Initialise();
#endif

#ifdef __WXMSW__
    if (wxTheApp->GetComCtl32Version() >= 600 && ::wxDisplayDepth() >= 32)
    {
        // Disable remapping of toolbar icons' colors to system colors,
        // so that alpha channel is displayed correctly (not brightened).
        wxSystemOptions::SetOption("msw.remap", 2);

        // NOTE: for some reason the above is done automatically by MinGW-built executables,
        // but not by MSVC (18.00) ones (where we use /MANIFEST:EMBED option), even though
        // both manifests (the MSVC-generated one and the one from <WX_ROOT>/include/wx/msw,
        // used for MinGW builds) have the same contents.
    }
#endif

    wxImage::AddHandler(new wxPNGHandler()); // Needed for loading the toolbar icons

    // Disable built-in wxWidgets logging. Otherwise the accumulated messages, e.g. of XML parsing errors,
    // would be automatically shown in message boxes when opening any modal dialog.
    wxLog::EnableLogging(false);

    // Location of the configuration file is platform-dependent. On recent 64-bit MS Windows it is
    // %HOMEPATH%\AppData\Roaming\imppg.ini. On Linux: ~/.imppg.
    m_AppConfig = new wxFileConfig("imppg", wxEmptyString, wxEmptyString, wxEmptyString, wxCONFIG_USE_LOCAL_FILE);
    Configuration::Initialize(m_AppConfig);

    if (Configuration::OpenGLInitIncomplete)
    {
        wxMessageBox(_("OpenGL back end failed to initialize when ImPPG was last started. Reverting to CPU + bitmaps mode."), _("Warning"), wxICON_WARNING | wxOK);
        Configuration::ProcessingBackEnd = BackEnd::CPU_AND_BITMAPS;
        Configuration::OpenGLInitIncomplete = false;
        m_AppConfig->Flush();
    }

    // Setup the UI language to use
    m_Language = wxLANGUAGE_DEFAULT;
    wxString requestedLangCode = Configuration::UiLanguage;
    if (requestedLangCode != wxEmptyString && requestedLangCode != Configuration::USE_DEFAULT_SYS_LANG)
    {
        const wxLanguageInfo* langInfo = wxLocale::FindLanguageInfo(requestedLangCode);
        if (langInfo)
            m_Language = static_cast<wxLanguage>(langInfo->Language);
    }
    wxFileTranslationsLoader::AddCatalogLookupPathPrefix("./lang");
    m_Locale.Init(m_Language);
    m_Locale.AddCatalog("wxstd3"); ///< The wxWidgets catalog (translated captions of standard menu items like "Open", control buttons like "Browse" etc.)
    m_Locale.AddCatalog("imppg");

    // Initialize internal logging
    m_LogStream = 0;
    if (argc > 1 && argv[1] == "--log")
    {
        wxFileName logFilePath(wxFileName::GetHomeDir(), "imppg", "log");
        m_LogStream = new std::ofstream(logFilePath.GetFullPath().ToStdString().c_str(), std::ios_base::out | std::ios_base::app);
        Log::Initialize(Log::LogLevel::NORMAL, *m_LogStream);
        Log::Print(wxString("\n") + wxDateTime::Now().FormatISOCombined(' ') + " ------------ IMPPG STARTED ------------\n\n", false);
    }

    Cursors::InitCursors();
    wxToolTip::Enable(true);
    c_MainWindow* mainWnd = new c_MainWindow();

    wxIcon appIcon;
    appIcon.CopyFromBitmap(LoadBitmap("imppg-app"));
    mainWnd->SetIcon(appIcon);
    mainWnd->Show();
    SetTopWindow(mainWnd);

    return true;
}

int c_MyApp::OnExit()
{
    if (m_LogStream)
    {
        Log::Print("Exiting\n");
        m_LogStream->close();
        delete m_LogStream;
    }

    delete m_AppConfig;

#if USE_FREEIMAGE
    FreeImage_DeInitialise();
#endif

    return 0;
}

void c_MyApp::OnUnhandledException()
{
    try { throw; }
    catch(std::exception& exc)
    {
        CRASH_LOG << "Exception: " << exc.what() << std::endl;
        wxMessageBox(wxString("Fatal exception:\n\n") + exc.what(), "Error", wxICON_ERROR | wxCLOSE);
        throw;
    }
}
