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
    Scrollable view class header.
*/

#ifndef IMPPG_SCROLLVIEW_HEADER
#define IMPPG_SCROLLVIEW_HEADER

#include <functional>
#include <wx/panel.h>
#include <wx/scrolbar.h>

/// Scrolled display window similar to `wxScrolled`, but with customized handling of scrolling.
class c_ScrolledView: public wxPanel
{
public:
    c_ScrolledView(wxWindow* parent = nullptr);

    /// Returns panel to be used for adding child controls, if any, and for binding event handlers.
    wxPanel& GetContentsPanel() { return *m_Contents; }

    /// Sets the logical view size (similarly to `wxScrolled::SetVirtualSize`).
    void SetActualSize(unsigned width, unsigned height);

    void SetActualSize(const wxSize& size) { SetActualSize(size.x, size.y); }

    void BindScrollCallback(std::function<void()> callback) { m_ScrollCallback = callback; }

    void ScrollTo(wxPoint position);

    /// Converts physical position to logical one.
    wxPoint CalcUnscrolledPosition(const wxPoint& pos) const
    {
        return wxPoint{ pos.x + m_ScrollPos.x, pos.y + m_ScrollPos.y };
    }

    /// Converts logical position to physical one.
    wxPoint CalcScrolledPosition(const wxPoint& pos) const
    {
        return wxPoint{ pos.x - m_ScrollPos.x, pos.y - m_ScrollPos.y };
    }

    bool Create(
        wxWindow* parent,
        wxWindowID winid = wxID_ANY,
        const wxPoint& pos = wxDefaultPosition,
        const wxSize& size = wxDefaultSize,
        long style = wxTAB_TRAVERSAL | wxNO_BORDER,
        const wxString& name = wxPanelNameStr
    ) = delete;

private:
    wxPoint m_ScrollPos{0, 0};
    unsigned m_ActualWidth{0};
    unsigned m_ActualHeight{0};
    wxScrollBar* m_VertSBar{nullptr};
    wxScrollBar* m_HorzSBar{nullptr};
    wxPanel* m_Contents{nullptr};
    std::function<void()> m_ScrollCallback;

    void UpdateScrollBars();
};

#endif // IMPPG_SCROLLVIEW_HEADER
