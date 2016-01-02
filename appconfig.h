/*
ImPPG (Image Post-Processor) - common operations for astronomical stacks and other images
Copyright (C) 2015, 2016 Filip Szczerek <ga.software@yahoo.com>

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
    Application configuration getters/setters header.
*/

#ifndef IMPPG_APPCONFIG_H
#define IMPPG_APPCONFIG_H

#include <wx/fileconf.h>
#include <wx/gdicmn.h>
#include "formats.h"

namespace Configuration
{
    // Default maximum frequency (in Hz) of issuing new processing requests by the tone curve editor and numerical control sliders (0 means: no limit)
    const int DEFAULT_MAX_PROCESSING_REQUESTS_PER_SEC = 30;

    const wxString USE_DEFAULT_SYS_LANG = "default";

    void Initialize(wxFileConfig *appConfig);

    /// Returns maximum frequency (in Hz) of issuing new processing requests by the tone curve editor and numerical control sliders (0 means: no limit)
    int GetMaxProcessingRequestsPerSec();

    wxString GetFileOpenPath();
    void SetFileOpenPath(wxString path);

    wxString GetFileSavePath();
    void SetFileSavePath(wxString path);

    bool IsMainWindowMaximized();
    void SetMainWindowMaximized(bool maximized);

    wxRect GetMainWindowPosSize();
    void SetMainWindowPosSize(wxRect r);

    wxRect GetToneCurveEditorPosSize();
    void SetToneCurveEditorPosSize(wxRect r);

    bool GetToneCurveEditorVisible();
    void SetToneCurveEditorVisible(bool visible);

    wxString GetLoadSettingsPath();
    void SetLoadSettingsPath(wxString path);

    wxString GetSaveSettingsPath();
    void SetSaveSettingsPath(wxString path);

    wxString GetBatchFileOpenPath();
    void SetBatchFileOpenPath(wxString path);

    wxString GetBatchLoadSettingsPath();
    void SetBatchLoadSettingsPath(wxString path);

    wxString GetBatchOutputPath();
    void SetBatchOutputPath(wxString path);

    wxRect GetBatchDialogPosSize();
    void SetBatchDialogPosSize(wxRect r);

    wxRect GetBatchProgressDialogPosSize();
    void SetBatchProgressDialogPosSize(wxRect r);

    wxRect GetAlignProgressDialogPosSize();
    void SetAlignProgressDialogPosSize(wxRect r);

    wxString GetAlignInputPath();
    void SetAlignInputPath(wxString path);

    wxString GetAlignOutputPath();
    void SetAlignOutputPath(wxString path);

    wxRect GetAlignParamsDialogPosSize();
    void SetAlignParamsDialogPosSize(wxRect r);

    /// Returns code of UI language to use or an empty string (then the system default will used)
    wxString GetUiLanguage();
    void SetUiLanguage(wxString lang);

    bool GetLogHistogram();
    void SetLogHistogram(bool value);

    OutputFormat_t GetBatchOutputFormat();
    void SetBatchOutputFormat(OutputFormat_t outFmt);

    int GetProcessingPanelWidth();
    void SetProcessingPanelWidth(int width);
}

#endif
