/*
ImPPG (Image Post-Processor) - common operations for astronomical stacks and other images
Copyright (C) 2016-2017 Filip Szczerek <ga.software@yahoo.com>

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

#include <wx/bitmap.h>
#include <wx/window.h>

template<typename T>
struct strPoint
{
    T x, y;

    strPoint() : x(0), y(0) { }
    strPoint(T x, T y) : x(x), y(y) { }

    // Used for binary search via std::lower_bound()
    bool operator <(const strPoint &p) const { return x < p.x; }
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

    void operator =(const T &value) { m_Setter(value); }
    operator T() const { return m_Getter(); }
};

class IValidatedInput
{
public:
    virtual bool HasCorrectInput() = 0;
};


#define SQR(x) ((x)*(x))

/// Checks if a window is visible on any display; if not, sets its size and position to default
void FixWindowPosition(wxWindow &wnd);

/// Loads a bitmap from the "images" subdirectory, optionally scaling it
wxBitmap LoadBitmap(wxString name, bool scale = false, wxSize scaledSize = wxDefaultSize);

template<class T>
void BindAllScrollEvents(wxEvtHandler &evtHandler, void(T::* evtFunc)(wxScrollWinEvent &), T *parent)
{
    for (auto tag: { wxEVT_SCROLLWIN_BOTTOM,
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

#endif //  IMPGG_COMMON_HEADER
