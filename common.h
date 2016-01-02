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
    Common declarations.
*/

#ifndef IMPGG_COMMON_HEADER
#define IMPGG_COMMON_HEADER

#include <wx/window.h>

template<typename T>
struct strPoint
{
    T x, y;

    strPoint() : x(0), y(0) { }
    strPoint(T x, T y) : x(x), y(y) { }

    // Used for binary search via std::lower_bound()
    bool operator <(const strPoint &p) { return x < p.x; }
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

#define SQR(x) ((x)*(x))

/// Checks if a window is visible on any display; if not, sets its size and position to default
void FixWindowPosition(wxWindow *wnd);

#endif //  IMPGG_COMMON_HEADER
