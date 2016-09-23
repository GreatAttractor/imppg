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
    Application configuration getters/setters implementation.
*/

#include <sstream>
#include <wx/gdicmn.h>
#include <wx/filename.h>
#include "appconfig.h"

bool wxFromString(const wxString &string, wxRect *rect)
{
    std::stringstream parser;
    parser << string;

    char sep; // separator
    parser >> (rect->x) >> sep >>
              (rect->y) >> sep >>
              (rect->width) >> sep >>
              (rect->height) >> sep;

    if (parser.fail())
        rect->x = rect->y = rect->width = rect->height = -1;

    return !parser.fail();
}

wxString wxToString(const wxRect &r)
{
    return wxString::Format("%d;%d;%d;%d;", r.x, r.y, r.width, r.height);
}

namespace Configuration
{

wxFileConfig *appConfig;

namespace Keys
{
    const char *FILE_OPEN_PATH =         "/UserInterface/FileOpenPath";
    const char *FILE_SAVE_PATH =         "/UserInterface/FileSavePath";
    const char *MAIN_WND_POS_SIZE =      "/UserInterface/MainWindowPosSize";
    const char *MAIN_WND_MAXIMIZED =     "/UserInterface/MainWindowMaximized";
    const char *TCURVE_EDITOR_POS_SIZE = "/UserInterface/ToneCurveEditorPosSize";
    const char *TCURVE_EDITOR_VISIBLE =  "/UserInterface/ToneCurveEditorVisible";
    const char *SAVE_SETTINGS_PATH =     "/UserInterface/SaveSettingsPath";
    const char *LOAD_SETTINGS_PATH =     "/UserInterface/LoadSettingsPath";
    const char *PROC_PANEL_WIDTH =       "/UserInterace/ProcessingPanelWidth";

    const char *BATCH_FILES_OPEN_PATH =       "/UserInterface/BatchFilesOpenPath";
    const char *BATCH_LOAD_SETTINGS_PATH =    "/UserInterface/BatchLoadSettingsPath";
    const char *BATCH_OUTPUT_PATH =           "/UserInterface/BatchOutputPath";
    const char *BATCH_DLG_POS_SIZE =          "/UserInterface/BatchDlgPosSize";
    const char *BATCH_PROGRESS_DLG_POS_SIZE = "/UserInterface/BatchProgressDlgPosSize";
    const char *BATCH_OUTPUT_FORMAT =         "/UserInterface/BatchOutputFormat";


    const char *ALIGN_INPUT_PATH =            "/UserInterface/AlignInputPath";
    const char *ALIGN_OUTPUT_PATH =           "/UserInterface/AlignOutputPath";
    const char *ALIGN_PROGRESS_DLG_POS_SIZE = "/UserInterface/AlignProgressDlgPosSize";
    const char *ALIGN_PARAMS_DLG_POS_SIZE =   "/UserInterface/AlignParamsDlgPosSize";

    /// Indicates maximum frequency (in Hz) of issuing new processing requests by tone curve editor and numerical control sliders
    const char *MAX_PROCESSING_REQUESTS_PER_SEC =  "/UserInterface/MaxProcessingRequestsPerSecond";

    /// Standard language code (e.g. "en", "pl") or Configuration::USE_DEFAULT_SYS_LANG for the system default
    const char *UI_LANGUAGE = "/UserInterface/Language";

    const char *LOGARITHMIC_HISTOGRAM =   "/UserInterface/LogHistogram";
}

void Initialize(wxFileConfig *_appConfig)
{
    appConfig = _appConfig;
}

/// Returns maximum frequency (in Hz) of issuing new processing requests by the tone curve editor and numerical control sliders (0 means: no limit)
int GetMaxProcessingRequestsPerSec()
{
    int val = (int)appConfig->ReadLong(Keys::MAX_PROCESSING_REQUESTS_PER_SEC, DEFAULT_MAX_PROCESSING_REQUESTS_PER_SEC);
    if (val < 0)
        val = DEFAULT_MAX_PROCESSING_REQUESTS_PER_SEC;
    if (val == DEFAULT_MAX_PROCESSING_REQUESTS_PER_SEC)
        appConfig->Write(Keys::MAX_PROCESSING_REQUESTS_PER_SEC, DEFAULT_MAX_PROCESSING_REQUESTS_PER_SEC);
    return val;
}

wxString GetFileOpenPath()
{
    return appConfig->Read(Keys::FILE_OPEN_PATH, "");
}

void SetFileOpenPath(wxString path)
{
    appConfig->Write(Keys::FILE_OPEN_PATH, path);
}

wxString GetFileSavePath()
{
    return appConfig->Read(Keys::FILE_SAVE_PATH, "");
}

void SetFileSavePath(wxString path)
{
    appConfig->Write(Keys::FILE_SAVE_PATH, path);
}

bool IsMainWindowMaximized()
{
    return appConfig->ReadBool(Keys::MAIN_WND_MAXIMIZED, false);
}

void SetMainWindowMaximized(bool maximized)
{
    appConfig->Write(Keys::MAIN_WND_MAXIMIZED, maximized);
}

wxRect GetMainWindowPosSize()
{
    wxRect result;
    appConfig->Read(Keys::MAIN_WND_POS_SIZE, &result, wxRect(-1, -1, -1, -1));
    return result;

}

void SetMainWindowPosSize(wxRect r)
{
    appConfig->Write(Keys::MAIN_WND_POS_SIZE, r);
}

wxRect GetToneCurveEditorPosSize()
{
    wxRect result;
    appConfig->Read(Keys::TCURVE_EDITOR_POS_SIZE, &result, wxRect(-1, -1, -1, -1));
    return result;
}

void SetToneCurveEditorPosSize(wxRect r)
{
    appConfig->Write(Keys::TCURVE_EDITOR_POS_SIZE, r);
}

bool GetToneCurveEditorVisible()
{
    bool result = true;
    appConfig->Read(Keys::TCURVE_EDITOR_VISIBLE, &result, true);
    return result;
}

void SetToneCurveEditorVisible(bool visible)
{
    appConfig->Write(Keys::TCURVE_EDITOR_VISIBLE, visible);
}

wxString GetSaveSettingsPath()
{
    return appConfig->Read(Keys::SAVE_SETTINGS_PATH, wxFileName::GetHomeDir());
}

void SetSaveSettingsPath(wxString path)
{
    appConfig->Write(Keys::SAVE_SETTINGS_PATH, path);
}

wxString GetLoadSettingsPath()
{
    return appConfig->Read(Keys::LOAD_SETTINGS_PATH, wxFileName::GetHomeDir());
}

void SetLoadSettingsPath(wxString path)
{
    appConfig->Write(Keys::LOAD_SETTINGS_PATH, path);
}

wxString GetBatchFileOpenPath()
{
    return appConfig->Read(Keys::BATCH_FILES_OPEN_PATH, GetFileOpenPath());
}

void SetBatchFileOpenPath(wxString path)
{
    appConfig->Write(Keys::BATCH_FILES_OPEN_PATH, path);
}

wxString GetBatchLoadSettingsPath()
{
    return appConfig->Read(Keys::BATCH_LOAD_SETTINGS_PATH, GetLoadSettingsPath());
}

void SetBatchLoadSettingsPath(wxString path)
{
    appConfig->Write(Keys::BATCH_LOAD_SETTINGS_PATH, path);
}

wxString GetBatchOutputPath()
{
    return appConfig->Read(Keys::BATCH_OUTPUT_PATH, GetFileSavePath());
}

void SetBatchOutputPath(wxString path)
{
    appConfig->Write(Keys::BATCH_OUTPUT_PATH, path);
}

wxRect GetBatchDialogPosSize()
{
    wxRect result;
    appConfig->Read(Keys::BATCH_DLG_POS_SIZE, &result, wxRect(-1, -1, -1, -1));
    return result;
}

void SetBatchDialogPosSize(wxRect r)
{
    appConfig->Write(Keys::BATCH_DLG_POS_SIZE, r);
}

wxRect GetBatchProgressDialogPosSize()
{
    wxRect result;
    appConfig->Read(Keys::BATCH_PROGRESS_DLG_POS_SIZE, &result, wxRect(-1, -1, -1, -1));
    return result;
}

void SetBatchProgressDialogPosSize(wxRect r)
{
    appConfig->Write(Keys::BATCH_PROGRESS_DLG_POS_SIZE, r);
}

wxRect GetAlignProgressDialogPosSize()
{
    wxRect result;
    appConfig->Read(Keys::ALIGN_PROGRESS_DLG_POS_SIZE, &result, wxRect(-1, -1, -1, -1));
    return result;
}

void SetAlignProgressDialogPosSize(wxRect r)
{
    appConfig->Write(Keys::ALIGN_PROGRESS_DLG_POS_SIZE, r);
}

wxString GetAlignInputPath()
{
    return appConfig->Read(Keys::ALIGN_INPUT_PATH);
}

void SetAlignInputPath(wxString path)
{
    appConfig->Write(Keys::ALIGN_INPUT_PATH, path);
}

wxString GetAlignOutputPath()
{
    return appConfig->Read(Keys::ALIGN_OUTPUT_PATH);
}

void SetAlignOutputPath(wxString path)
{
    appConfig->Write(Keys::ALIGN_OUTPUT_PATH, path);
}

wxRect GetAlignParamsDialogPosSize()
{
    wxRect result;
    appConfig->Read(Keys::ALIGN_PARAMS_DLG_POS_SIZE, &result, wxRect(-1, -1, -1, -1));
    return result;
}

void SetAlignParamsDialogPosSize(wxRect r)
{
    appConfig->Write(Keys::ALIGN_PARAMS_DLG_POS_SIZE, r);
}

/// Returns code of UI language to use or an empty string (then the system default will used)
wxString GetUiLanguage()
{
    return appConfig->Read(Keys::UI_LANGUAGE, wxEmptyString);
}

void SetUiLanguage(wxString lang)
{
    appConfig->Write(Keys::UI_LANGUAGE, lang);
}

bool GetLogHistogram()
{
    return appConfig->ReadBool(Keys::LOGARITHMIC_HISTOGRAM, false);
}

void SetLogHistogram(bool value)
{
    appConfig->Write(Keys::LOGARITHMIC_HISTOGRAM, value);
}

OutputFormat_t GetBatchOutputFormat()
{
    long result = appConfig->ReadLong(Keys::BATCH_OUTPUT_FORMAT, (long)OUTF_BMP_MONO_8);
    if (result < 0 || result >= OUTF_LAST)
        result = OUTF_BMP_MONO_8;

    return (OutputFormat_t)result;
}

void SetBatchOutputFormat(OutputFormat_t outFmt)
{
    appConfig->Write(Keys::BATCH_OUTPUT_FORMAT, (long)outFmt);
}

int GetProcessingPanelWidth()
{
    return (int)appConfig->ReadLong(Keys::PROC_PANEL_WIDTH, -1);
}

void SetProcessingPanelWidth(int width)
{
    appConfig->Write(Keys::PROC_PANEL_WIDTH, width);
}


}
