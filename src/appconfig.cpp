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
    Application configuration getters/setters implementation.
*/

#include <sstream>
#include <wx/gdicmn.h>
#include <wx/filename.h>

#include "appconfig.h"
#include "common.h"
#include "imppg_assert.h"

static bool wxFromString(const wxString& string, wxRect* rect)
{
    std::stringstream parser;
    parser << string.ToStdString();

    char sep; // separator
    parser >> (rect->x) >> sep >>
              (rect->y) >> sep >>
              (rect->width) >> sep >>
              (rect->height) >> sep;

    if (parser.fail())
        rect->x = rect->y = rect->width = rect->height = -1;

    return !parser.fail();
}

static wxString wxToString(const wxRect& r)
{
    return wxString::Format("%d;%d;%d;%d;", r.x, r.y, r.width, r.height);
}

namespace Configuration
{

wxFileConfig* appConfig{nullptr};

namespace Keys
{
#define UserInterfaceGroup "/UserInterface"

    const char* FileOpenPath           = UserInterfaceGroup"/FileOpenPath";
    const char* FileSavePath           = UserInterfaceGroup"/FileSavePath";
    const char* MainWindowPosSize      = UserInterfaceGroup"/MainWindowPosSize";
    const char* MainWindowMaximized    = UserInterfaceGroup"/MainWindowMaximized";
    const char* ToneCurveEditorPosSize = UserInterfaceGroup"/ToneCurveEditorPosSize";
    const char* ToneCurveEditorVisible = UserInterfaceGroup"/ToneCurveEditorVisible";
    const char* SaveSettingsPath       = UserInterfaceGroup"/SaveSettingsPath";
    const char* LoadSettingsPath       = UserInterfaceGroup"/LoadSettingsPath";
    const char* ProcessingPanelWidth   = UserInterfaceGroup"/ProcessingPanelWidth";
    const char* ToolIconSize           = UserInterfaceGroup"/ToolIconSize";
    const char* ToneCurveEditorNumDrawSegments = UserInterfaceGroup"/ToneCurveEditorNumDrawSegments";
    const char* ToneCurveEditorColors  = UserInterfaceGroup"/ToneCurveEditorColors";
    const char* FileOutputFormat       = UserInterfaceGroup"/FileOutputFormat";
    const char* FileInputFormatIndex   = UserInterfaceGroup"/FileInputFormatIndex";

    const char* ToneCurveEditor_CurveColor                 = UserInterfaceGroup"/ToneCurveEditor_CurveColor";
    const char* ToneCurveEditor_BackgroundColor            = UserInterfaceGroup"/ToneCurveEditor_BackgroundColor";
    const char* ToneCurveEditor_CurvePointColor            = UserInterfaceGroup"/ToneCurveEditor_CurvePointColor";
    const char* ToneCurveEditor_SelectedCurvePointColor    = UserInterfaceGroup"/ToneCurveEditor_SelectedCurvePointColor";
    const char* ToneCurveEditor_HistogramColor             = UserInterfaceGroup"/ToneCurveEditor_HistogramColor";
    const char* ToneCurveEditor_CurveWidth                 = UserInterfaceGroup"/ToneCurveEditor_CurveWidth";
    const char* ToneCurveEditor_CurvePointSize             = UserInterfaceGroup"/ToneCurveEditor_CurvePointSize";
    const char* ToneCurveSettingsDialogPosSize             = UserInterfaceGroup"/ToneCurveSettingsDialogPosSize";

    const char* BatchFileOpenPath           = UserInterfaceGroup"/BatchFilesOpenPath";
    const char* BatchLoadSettingsPath       = UserInterfaceGroup"/BatchLoadSettingsPath";
    const char* BatchOutputPath             = UserInterfaceGroup"/BatchOutputPath";
    const char* BatchDialogPosSize          = UserInterfaceGroup"/BatchDlgPosSize";
    const char* BatchProgressDialogPosSize  = UserInterfaceGroup"/BatchProgressDlgPosSize";
    const char* BatchOutputFormat           = UserInterfaceGroup"/BatchOutputFormat";


    const char* AlignInputPath              = UserInterfaceGroup"/AlignInputPath";
    const char* AlignOutputPath             = UserInterfaceGroup"/AlignOutputPath";
    const char* AlignProgressDialogPosSize  = UserInterfaceGroup"/AlignProgressDlgPosSize";
    const char* AlignParamsDialogPosSize    = UserInterfaceGroup"/AlignParamsDlgPosSize";

    /// Indicates maximum frequency (in Hz) of issuing new processing requests by tone curve editor and numerical control sliders
    const char* MAX_PROCESSING_REQUESTS_PER_SEC =  UserInterfaceGroup"/MaxProcessingRequestsPerSecond";

    /// Standard language code (e.g. "en", "pl") or Configuration::USE_DEFAULT_SYS_LANG for the system default
    const char* UiLanguage = UserInterfaceGroup"/Language";

    /// Histogram values in logarithmic scale
    const char* LogHistogram =   UserInterfaceGroup"/LogHistogram";

    /// Most recently used settings files
    const char* MruSettingsGroup = "/MostRecentSettings";
    const char* MruSettingsItem = "item";

    const char* ProcessingBackEnd = "/BackEnd";

    const char* DisplayScalingMethod = UserInterfaceGroup"/DisplayScalingMethod";

#define OpenGLGroup "/OpenGL"

    const char* LRCmdBatchSizeMpixIters = OpenGLGroup"/LRCommandBatchSizeMpixIters";
    const char* OpenGLInitIncomplete = OpenGLGroup"/OpenGLInitIncomplete";

    const char* NormalizeFITSValues = "/NormalizeFITSValues";
}

void Initialize(wxFileConfig* _appConfig)
{
    appConfig = _appConfig;
}

void Flush()
{
    IMPPG_ASSERT(nullptr != appConfig);
    appConfig->Flush();
}

/// Returns maximum frequency (in Hz) of issuing new processing requests by the tone curve editor and numerical control sliders (0 means: no limit)
int GetMaxProcessingRequestsPerSec()
{
    int val = static_cast<int>(appConfig->ReadLong(Keys::MAX_PROCESSING_REQUESTS_PER_SEC, DEFAULT_MAX_PROCESSING_REQUESTS_PER_SEC));
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

#define PROPERTY_BOOL(Name, DefaultValue)                               \
    c_Property<bool> Name(                                              \
        []() { return appConfig->ReadBool(Keys::Name, DefaultValue); }, \
        [](const bool &val) { appConfig->Write(Keys::Name, val); }      \
    )

PROPERTY_BOOL(MainWindowMaximized, true);
PROPERTY_BOOL(ToneCurveEditorVisible, true);
PROPERTY_BOOL(LogHistogram, true);

PROPERTY_BOOL(OpenGLInitIncomplete, false);

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
PROPERTY_RECT(ToneCurveSettingsDialogPosSize);

// Finds and uses wxFromString() and wxToString() defined above
#define PROPERTY_OUTPUT_FORMAT(Name)                                                                       \
    c_Property<OutputFormat> Name(                                                                         \
        []()                                                                                               \
        {                                                                                                  \
            long result = appConfig->ReadLong(Keys::Name, static_cast<long>(OutputFormat::BMP_8));         \
            if (result < 0 || result >= static_cast<long>(OutputFormat::LAST))                             \
                return OutputFormat::BMP_8;                                                                \
                                                                                                           \
            return static_cast<OutputFormat>(result);                                                      \
        },                                                                                                 \
        [](const OutputFormat &val) { appConfig->Write(Keys::Name, static_cast<long>(val)); }              \
    )

PROPERTY_OUTPUT_FORMAT(FileOutputFormat);
PROPERTY_OUTPUT_FORMAT(BatchOutputFormat);

c_Property<ToneCurveEditorColors> ToneCurveColors(
    []()
    {
        long result = static_cast<long>(appConfig->ReadLong(Keys::ToneCurveEditorColors, static_cast<long>(ToneCurveEditorColors::ImPPGDefaults)));
        if (result < 0 || result >= static_cast<long>(ToneCurveEditorColors::Last))
            return ToneCurveEditorColors::ImPPGDefaults;

        return static_cast<ToneCurveEditorColors>(result);
    },
    [](const ToneCurveEditorColors &val) { appConfig->Write(Keys::ToneCurveEditorColors, static_cast<long>(val)); }
);

c_Property<BackEnd> ProcessingBackEnd(
    []()
    {
        long result = appConfig->ReadLong(Keys::ProcessingBackEnd,
#if USE_OPENGL_BACKEND
            static_cast<long>(BackEnd::GPU_OPENGL)
#else
            static_cast<long>(BackEnd::CPU_AND_BITMAPS)
#endif
        );
        if (result < 0 ||
           result >= static_cast<long>(BackEnd::LAST) ||
           result == static_cast<long>(BackEnd::GPU_OPENGL) && !USE_OPENGL_BACKEND)
        {
            return BackEnd::CPU_AND_BITMAPS;
        }
        return static_cast<BackEnd>(result);
    },
    [](const BackEnd& val) { appConfig->Write(Keys::ProcessingBackEnd, static_cast<long>(val)); }
);

#define PROPERTY_TCRV_COLOR(Name, DefaultValue)                                                           \
    c_Property<wxColour> Name(                                                                            \
        [](){                                                                                             \
            switch (ToneCurveColors)                                                                      \
            {                                                                                             \
                case ToneCurveEditorColors::ImPPGDefaults: return DefaultColors::ImPPG::DefaultValue;     \
                case ToneCurveEditorColors::SystemDefaults: return DefaultColors::System::DefaultValue(); \
                case ToneCurveEditorColors::Custom:                                                       \
                    {                                                                                     \
                        wxColour result;                                                                  \
                        appConfig->Read(Keys::Name, &result, DefaultColors::ImPPG::DefaultValue);         \
                        return result;                                                                    \
                    }                                                                                     \
                default: return DefaultColors::ImPPG::DefaultValue;                                       \
            }                                                                                             \
        },                                                                                                \
        [](const wxColour &c) { appConfig->Write(Keys::Name, c); }                                        \
    );                                                                                                    \

PROPERTY_TCRV_COLOR(ToneCurveEditor_CurveColor, Curve)
PROPERTY_TCRV_COLOR(ToneCurveEditor_BackgroundColor, CurveBackground)
PROPERTY_TCRV_COLOR(ToneCurveEditor_CurvePointColor, CurvePoint)
PROPERTY_TCRV_COLOR(ToneCurveEditor_SelectedCurvePointColor, SelectedCurvePoint)
PROPERTY_TCRV_COLOR(ToneCurveEditor_HistogramColor, Histogram)

#define PROPERTY_UNSIGNED(Name, DefaultValue)                                                 \
    c_Property<unsigned> Name(                                                                \
        [](){ return static_cast<unsigned>(appConfig->ReadLong(Keys::Name, DefaultValue)); }, \
        [](const unsigned &u) { appConfig->Write(Keys::Name, u); }                            \
    )

#define PROPERTY_INT(Name, DefaultValue)                                                 \
    c_Property<int> Name(                                                                \
        [](){ return static_cast<int>(appConfig->ReadLong(Keys::Name, DefaultValue)); }, \
        [](const int &i) { appConfig->Write(Keys::Name, i); }                            \
    )

PROPERTY_UNSIGNED(ToneCurveEditor_CurveWidth, 1);
PROPERTY_UNSIGNED(ToneCurveEditor_CurvePointSize, 4);
PROPERTY_INT(ProcessingPanelWidth, -1);
PROPERTY_UNSIGNED(ToolIconSize, DEFAULT_TOOL_ICON_SIZE);
PROPERTY_UNSIGNED(ToneCurveEditorNumDrawSegments, DEFAULT_TONE_CURVE_EDITOR_NUM_DRAW_SEGMENTS);
PROPERTY_INT(FileInputFormatIndex, 0);

/// Returns a list of the most recently used saved/loaded settings files
wxArrayString GetMruSettings()
{
    size_t index = 1;
    wxString val;
    wxArrayString result;
    while (index <= MAX_MRU_SETTINGS_ITEMS
           && appConfig->Read(wxString::Format("%s/%s%d", Keys::MruSettingsGroup, Keys::MruSettingsItem, static_cast<int>(index)), &val))
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
        appConfig->Write(wxString::Format("%s/%s%d", Keys::MruSettingsGroup, Keys::MruSettingsItem, static_cast<int>(i)+1), settings[i]);
}

void EmptyMruList()
{
    appConfig->DeleteGroup(Keys::MruSettingsGroup);
}

PROPERTY_UNSIGNED(LRCmdBatchSizeMpixIters, 1);

c_Property<ScalingMethod> DisplayScalingMethod(
    []()
    {
        long result = appConfig->ReadLong(Keys::DisplayScalingMethod, static_cast<long>(ScalingMethod::CUBIC));
        if (result < 0 ||
           result >= static_cast<long>(ScalingMethod::NUM_METHODS))
        {
            return ScalingMethod::CUBIC;
        }
        return static_cast<ScalingMethod>(result);
    },
    [](const ScalingMethod& val) { appConfig->Write(Keys::DisplayScalingMethod, static_cast<long>(val)); }
);

PROPERTY_BOOL(NormalizeFITSValues, true);

}  // namespace Configuration
