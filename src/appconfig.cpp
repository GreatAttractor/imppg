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
#define UserInterfaceGroup "/UserInterface"

    const char *FileOpenPath =           UserInterfaceGroup"/FileOpenPath";
    const char *FileSavePath =           UserInterfaceGroup"/FileSavePath";
    const char *MainWindowPosSize =      UserInterfaceGroup"/MainWindowPosSize";
    const char *MainWindowMaximized =    UserInterfaceGroup"/MainWindowMaximized";
    const char *ToneCurveEditorPosSize = UserInterfaceGroup"/ToneCurveEditorPosSize";
    const char *ToneCurveEditorVisible = UserInterfaceGroup"/ToneCurveEditorVisible";
    const char *SaveSettingsPath =       UserInterfaceGroup"/SaveSettingsPath";
    const char *LoadSettingsPath =       UserInterfaceGroup"/LoadSettingsPath";
    const char *ProcessingPanelWidth =   UserInterfaceGroup"/ProcessingPanelWidth";
    const char *ToolIconSize =           UserInterfaceGroup"/ToolIconSize";
    const char *ToneCurveEditorNumDrawSegments = UserInterfaceGroup"/ToneCurveEditorNumDrawSegments";

    const char *BatchFileOpenPath =          UserInterfaceGroup"/BatchFilesOpenPath";
    const char *BatchLoadSettingsPath =      UserInterfaceGroup"/BatchLoadSettingsPath";
    const char *BatchOutputPath =            UserInterfaceGroup"/BatchOutputPath";
    const char *BatchDialogPosSize =         UserInterfaceGroup"/BatchDlgPosSize";
    const char *BatchProgressDialogPosSize = UserInterfaceGroup"/BatchProgressDlgPosSize";
    const char *BatchOutputFormat =          UserInterfaceGroup"/BatchOutputFormat";


    const char *AlignInputPath =             UserInterfaceGroup"/AlignInputPath";
    const char *AlignOutputPath =            UserInterfaceGroup"/AlignOutputPath";
    const char *AlignProgressDialogPosSize = UserInterfaceGroup"/AlignProgressDlgPosSize";
    const char *AlignParamsDialogPosSize =   UserInterfaceGroup"/AlignParamsDlgPosSize";

    /// Indicates maximum frequency (in Hz) of issuing new processing requests by tone curve editor and numerical control sliders
    const char *MAX_PROCESSING_REQUESTS_PER_SEC =  UserInterfaceGroup"/MaxProcessingRequestsPerSecond";

    /// Standard language code (e.g. "en", "pl") or Configuration::USE_DEFAULT_SYS_LANG for the system default
    const char *UiLanguage = UserInterfaceGroup"/Language";

    /// Histogram values in logarithmic scale
    const char *LogHistogram =   UserInterfaceGroup"/LogHistogram";

    /// Most recently used settings files
    const char *MruSettingsGroup = "/MostRecentSettings";
    const char *MruSettingsItem = "item";
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

#define PROPERTY_STRING(Name)                                          \
    c_Property<wxString> Name(                                         \
        []() { return appConfig->Read(Keys::Name, ""); },              \
        [](const wxString &str) { appConfig->Write(Keys::Name, str); } \
    )

PROPERTY_STRING(FileOpenPath);
PROPERTY_STRING(FileSavePath);
PROPERTY_STRING(LoadSettingsPath);
PROPERTY_STRING(SaveSettingsPath);
PROPERTY_STRING(BatchFileOpenPath);
PROPERTY_STRING(BatchLoadSettingsPath);
PROPERTY_STRING(BatchOutputPath);
PROPERTY_STRING(AlignInputPath);
PROPERTY_STRING(AlignOutputPath);
PROPERTY_STRING(UiLanguage);

#define PROPERTY_BOOL(Name)                                        \
    c_Property<bool> Name(                                         \
        []() { return appConfig->ReadBool(Keys::Name, ""); },      \
        [](const bool &val) { appConfig->Write(Keys::Name, val); } \
    )

PROPERTY_BOOL(MainWindowMaximized);
PROPERTY_BOOL(ToneCurveEditorVisible);
PROPERTY_BOOL(LogHistogram);

// Finds and uses wxFromString() and wxToString() defined above
#define PROPERTY_RECT(Name)                                               \
    c_Property<wxRect> Name(                                              \
        []()                                                              \
        {                                                                 \
            wxRect result;                                                \
            appConfig->Read(Keys::Name, &result, wxRect(-1, -1, -1, -1)); \
            return result;                                                \
        },                                                                \
        [](const wxRect &r) { appConfig->Write(Keys::Name, r); }          \
    )

PROPERTY_RECT(MainWindowPosSize);
PROPERTY_RECT(ToneCurveEditorPosSize);
PROPERTY_RECT(BatchDialogPosSize);
PROPERTY_RECT(BatchProgressDialogPosSize);
PROPERTY_RECT(AlignProgressDialogPosSize);
PROPERTY_RECT(AlignParamsDialogPosSize);

c_Property<OutputFormat_t> BatchOutputFormat(
    []()
    {
        long result = appConfig->ReadLong(Keys::BatchOutputFormat, (long)OUTF_BMP_MONO_8);
        if (result < 0 || result >= OUTF_LAST)
            result = OUTF_BMP_MONO_8;

        return (OutputFormat_t)result;
    },
    [](const OutputFormat_t &val) { appConfig->Write(Keys::BatchOutputFormat, (long)val); }
);

c_Property<int> ProcessingPanelWidth(
    [](){ return (int)appConfig->ReadLong(Keys::ProcessingPanelWidth, -1); },
    [](const int &width) { appConfig->Write(Keys::ProcessingPanelWidth, width); }
);

c_Property<unsigned> ToolIconSize(
    [](){ return (unsigned)appConfig->ReadLong(Keys::ToolIconSize, DEFAULT_TOOL_ICON_SIZE); },
    [](const unsigned &ts) { appConfig->Write(Keys::ToolIconSize, ts); }
);

c_Property<unsigned> ToneCurveEditorNumDrawSegments(
    []()
    {
        unsigned num = (unsigned)appConfig->ReadLong(Keys::ToneCurveEditorNumDrawSegments, DEFAULT_TONE_CURVE_EDITOR_NUM_DRAW_SEGMENTS);
        if (DEFAULT_TONE_CURVE_EDITOR_NUM_DRAW_SEGMENTS == num)
            appConfig->Write(Keys::ToneCurveEditorNumDrawSegments, num);
        return num;
    },
    [](const unsigned &num) { appConfig->Write(Keys::ToneCurveEditorNumDrawSegments, num); }
);

/// Returns a list of the most recently used saved/loaded settings files
wxArrayString GetMruSettings()
{
    int index = 1;
    wxString val;
    wxArrayString result;
    while (index <= MAX_MRU_SETTINGS_ITEMS
           && appConfig->Read(wxString::Format("%s/%s%d", Keys::MruSettingsGroup, Keys::MruSettingsItem, index), &val))
    {
        result.Add(val);
        index++;
    }
    return result;
}

void StoreMruSettings(const wxArrayString &settings)
{
    appConfig->DeleteGroup(Keys::MruSettingsGroup);
    for (size_t i = 0; i < settings.Count(); i++)
        appConfig->Write(wxString::Format("%s/%s%zu", Keys::MruSettingsGroup, Keys::MruSettingsItem, i+1u), settings[i]);
}

void EmptyMruList()
{
    appConfig->DeleteGroup(Keys::MruSettingsGroup);
}

}
