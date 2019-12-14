#include <iostream>
#include <wx/dcclient.h>
#include <wx/sizer.h>
#include <wx/textctrl.h>

#include "scrolled_view.h"

c_ScrolledView::c_ScrolledView(wxWindow* parent): wxPanel(parent, wxID_ANY)
{
    auto* sizer = new wxFlexGridSizer(2);
    sizer->AddGrowableCol(0);
    sizer->AddGrowableRow(0);

    m_VertSBar = new wxScrollBar(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxSB_VERTICAL);
    m_VertSBar->Hide();

    m_HorzSBar = new wxScrollBar(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxSB_HORIZONTAL);
    m_HorzSBar->Hide();

    m_Contents = new wxPanel(this);

    sizer->Add(m_Contents, 1, wxGROW);
    sizer->Add(m_VertSBar, 0, wxGROW);
    sizer->Add(m_HorzSBar, 0, wxGROW);
    sizer->AddSpacer(0);

    SetSizer(sizer);
    Layout();

    Bind(wxEVT_SIZE, [this](wxSizeEvent& event)
    {
        UpdateScrollBars();
        m_ScrollPos = { m_HorzSBar->GetThumbPosition(), m_VertSBar->GetThumbPosition() };
        event.Skip();
    });

    auto OnScroll = [this](wxScrollEvent&)
    {
        const wxPoint oldScrollPos = m_ScrollPos;
        m_ScrollPos = { m_HorzSBar->GetThumbPosition(), m_VertSBar->GetThumbPosition() };

        if (m_WindowScrollingEnabled)
        {
            // IMPORTANT: when using CPU & bitmaps, cannot just call Refresh() here, because under wxMSW
            // repainting the whole window is relatively slow for large sizes (e.g., 4K screen).
            // Instead, call wxWindow::ScrollWindow(), which calls WinAPI's ScrollWindow(), which performs
            // a fast (apparently hardware-accelerated) scroll of the existing window contents without
            // destroying them, and only invalidates+repaints the newly revealed areas.
            // (The same applies to the call in c_ScrolledView::ScrollTo().)

            m_Contents->ScrollWindow(oldScrollPos.x - m_ScrollPos.x, oldScrollPos.y - m_ScrollPos.y);
        }
        else
        {
            m_Contents->Refresh();
        }
        
        if (m_ScrollCallback)
            m_ScrollCallback();
    };

    for (const auto tag: { wxEVT_SCROLL_TOP,
                           wxEVT_SCROLL_BOTTOM,
                           wxEVT_SCROLL_LINEUP,
                           wxEVT_SCROLL_LINEDOWN,
                           wxEVT_SCROLL_PAGEUP,
                           wxEVT_SCROLL_PAGEDOWN,
                           wxEVT_SCROLL_THUMBTRACK,
                           wxEVT_SCROLL_THUMBRELEASE })
    {
        m_HorzSBar->Bind(tag, OnScroll);
        m_VertSBar->Bind(tag, OnScroll);
    }
}

void c_ScrolledView::UpdateScrollBars()
{
    const auto displaySize = m_Contents->GetClientSize();

    if (m_ActualWidth > static_cast<unsigned>(displaySize.GetWidth()))
    {
        m_HorzSBar->Show();
        m_HorzSBar->SetScrollbar(m_ScrollPos.x, displaySize.GetWidth(), m_ActualWidth, m_ActualWidth / 10);
    }
    else
    {
        m_HorzSBar->Hide();
        m_ScrollPos.x = 0;
        m_HorzSBar->SetThumbPosition(m_ScrollPos.x);
    }

    if (m_ActualHeight > static_cast<unsigned>(displaySize.GetHeight()))
    {
        m_VertSBar->Show();
        m_VertSBar->SetScrollbar(m_ScrollPos.y, displaySize.GetHeight(), m_ActualHeight, m_ActualHeight / 10);
    }
    else
    {
        m_VertSBar->Hide();
        m_ScrollPos.y = 0;
        m_VertSBar->SetThumbPosition(m_ScrollPos.y);
    }
}

void c_ScrolledView::SetActualSize(unsigned width, unsigned height)
{
    if (m_ActualWidth != width || m_ActualHeight != height)
    {
        m_ActualWidth = width;
        m_ActualHeight = height;
        UpdateScrollBars();
        Layout();
    }
}

void c_ScrolledView::ScrollTo(wxPoint position)
{
    const wxPoint oldScrollPos = m_ScrollPos;
    const auto displaySize = m_Contents->GetClientSize();

    if (m_ActualWidth > static_cast<unsigned>(displaySize.GetWidth()))
    {
        m_ScrollPos.x = std::min(std::max(0, position.x), static_cast<int>(m_ActualWidth) - displaySize.x);
        m_HorzSBar->SetThumbPosition(position.x);
    }
    if (m_ActualHeight > static_cast<unsigned>(displaySize.GetHeight()))
    {
        m_ScrollPos.y = std::min(std::max(0, position.y), static_cast<int>(m_ActualHeight) - displaySize.y);
        m_VertSBar->SetThumbPosition(position.y);
    }

    if (m_WindowScrollingEnabled)
    {
        m_Contents->ScrollWindow(oldScrollPos.x - m_ScrollPos.x, oldScrollPos.y - m_ScrollPos.y);
    }
    else
    {
        m_Contents->Refresh();
    }
}
