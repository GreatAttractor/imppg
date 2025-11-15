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
    Common declarations.
*/

#ifndef IMPGG_COMMON_HEADER
#define IMPGG_COMMON_HEADER

#include "common/imppg_assert.h"

#include <array>
#include <filesystem>
#include <variant>
#include <vector>
#include <wx/bitmap.h>
#include <wx/filename.h>
#include <wx/settings.h>
#include <wx/window.h>

constexpr float ZOOM_NONE = 1.0f;

constexpr double MAX_GAUSSIAN_SIGMA = 10.0; // shaders/unsh_mask.frag uses the same value

constexpr float RAW_IMAGE_BLUR_SIGMA_FOR_ADAPTIVE_UNSHARP_MASK = 1.0f;

template<typename ... Ts>
struct Overload : Ts ... {
    using Ts::operator() ...;
};
template<class... Ts> Overload(Ts...) -> Overload<Ts...>;

template<typename T>
T& checked_back(std::vector<T>& v)
{
    IMPPG_ASSERT(!v.empty());
    return v.back();
}

template<typename T>
const T& checked_back(const std::vector<T>& v)
{
    IMPPG_ASSERT(!v.empty());
    return v.back();
}

class c_Image;

enum class ToneCurveEditorColors
{
    ImPPGDefaults = 0,
    SystemDefaults,
    Custom,

    Last
};

enum class ScalingMethod
{
    NEAREST = 0,
    LINEAR,
    CUBIC,

    NUM_METHODS // This has to be the last entry
};

namespace req_type
{
    struct Sharpening {};

    struct UnsharpMasking { std::size_t maskIdx; };

    struct ToneCurve {};
}

using ProcessingRequest = std::variant<
    req_type::Sharpening,
    req_type::UnsharpMasking,
    req_type::ToneCurve
>;

enum class BackEnd
{
    CPU_AND_BITMAPS = 0,
    GPU_OPENGL,

    LAST // this has to be the last entry
};

constexpr float DERINGING_BRIGHTNESS_THRESHOLD = 254.0f / 255;

namespace DefaultColors
{
namespace ImPPG
{
const wxColour Curve              = wxColour(0x000000UL);
const wxColour CurveBackground    = wxColour(0xFFFFFF);
const wxColour CurvePoint         = wxColour(0xAAAAAA);
const wxColour SelectedCurvePoint = wxColour(0x0000FF);
const wxColour Histogram          = wxColour(0xEEEEEE);
}
namespace System
{
inline wxColour Curve()           { return wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT); }
inline wxColour CurveBackground() { return wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW); }
inline wxColour CurvePoint()      { return wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT); }
inline wxColour SelectedCurvePoint() { return ImPPG::SelectedCurvePoint; }
inline wxColour Histogram()       { return wxSystemSettings::GetColour(wxSYS_COLOUR_BTNSHADOW); }
}
}

template<typename T>
struct strPoint
{
    T x, y;

    strPoint() : x(0), y(0) { }
    strPoint(T x, T y) : x(x), y(y) { }

    bool operator==(const strPoint& p) const { return x == p.x && y == p.y; }

    // Used for binary search via std::lower_bound()
    bool operator <(const strPoint& p) const { return x < p.x; }
};

typedef strPoint<int> Point_t;
typedef strPoint<float> FloatPoint_t;

// Same contents as wxRect; intended for use in wxWidgets-independent code
typedef struct strRectangle
{
    int x, y; /// coordinates of the xmin,ymin corner
    int width, height;

    strRectangle(): x(0), y(0), width(0), height(0) { }
    strRectangle(int x, int y, int width, int height): x(x), y(y), width(width), height(height) { }
} Rectangle_t;

/// Encapsulates a setter/getter of type T
template <typename T>
class c_Property
{
    T (*m_Getter)();
    void (*m_Setter)(const T&);

public:
    c_Property(T getter(), void setter(const T&))
      : m_Getter(getter), m_Setter(setter)
    { }

    void operator =(const T& value) { m_Setter(value); }
    operator T() const { return m_Getter(); }
};

class IValidatedInput
{
public:
    virtual bool HasCorrectInput() = 0;
};

/// Checks if a window is visible on any display; if not, sets its size and position to default
void FixWindowPosition(wxWindow& wnd);

/// Loads a bitmap from the "images" subdirectory, optionally scaling it
wxBitmap LoadBitmap(wxString name, bool scale = false, wxSize scaledSize = wxDefaultSize);

template<class T>
void BindAllScrollEvents(wxEvtHandler& evtHandler, void(T::* evtFunc)(wxScrollWinEvent&), T* parent)
{
    for (const auto tag: { wxEVT_SCROLLWIN_BOTTOM,
                           wxEVT_SCROLLWIN_LINEDOWN,
                           wxEVT_SCROLLWIN_LINEUP,
                           wxEVT_SCROLLWIN_PAGEDOWN,
                           wxEVT_SCROLLWIN_PAGEUP,
                           wxEVT_SCROLLWIN_THUMBRELEASE,
                           wxEVT_SCROLLWIN_THUMBTRACK,
                           wxEVT_SCROLLWIN_TOP })
     {
         evtHandler.Bind(tag, evtFunc, parent);
     }
}

/// Deleter for blocks allocated with `operator new[]`.
struct BlockDeleter { void operator()(void* ptr) { operator delete[](ptr); } };

struct Histogram
{
    float minValue; ///< Exact minimum value present in image
    float maxValue; ///< Exact maximum value present in image
    std::vector<int> values; ///< Histogram values for uniform intervals (bins)
    int maxCount; ///< Highest count among the histogram bins
};

Histogram DetermineHistogram(const c_Image& img, const wxRect& selection);

Histogram DetermineHistogramFromChannels(const std::vector<c_Image>& channels, const wxRect& selection);

wxString GetBackEndText(BackEnd backEnd);

inline std::filesystem::path ToFsPath(const wxString& wxs)
{
    using fspath = std::filesystem::path;
    return fspath{static_cast<const fspath::value_type*>(wxs.fn_str())};
}

inline std::filesystem::path FromDir(const wxFileName& dir, wxString fname)
{
    return ToFsPath(wxFileName(dir.GetFullPath(), fname).GetFullPath());
}

#endif //  IMPGG_COMMON_HEADER
