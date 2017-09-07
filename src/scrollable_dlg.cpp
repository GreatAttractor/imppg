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
    Scrollable dialog class implementation.
*/

#include <cassert>
#include "common.h"
#include "scrollable_dlg.h"


c_ScrollableDialog::c_ScrollableDialog(wxWindow *parent, wxWindowID id, wxString title)
    : wxDialog(parent, id, title, wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
      m_Contents(0), m_SzTop(0)
{
    SetExtraStyle(GetExtraStyle() | wxWS_EX_VALIDATE_RECURSIVELY); // Make sure all validators are run
}

c_ScrollableDialog::~c_ScrollableDialog()
{
}

void c_ScrollableDialog::InitControls(int borderSize)
{
    m_Contents = new wxScrolledWindow(this, wxID_ANY);
    BindAllScrollEvents(*m_Contents, &c_ScrollableDialog::OnContentsScrolled, this);
    m_Contents->Bind(wxEVT_SIZE, &c_ScrollableDialog::OnContentsResized, this);
    m_Contents->SetScrollRate(1, 1);

    m_SzTop = new wxBoxSizer(wxVERTICAL);
    m_SzTop->Add(m_Contents, 1, wxALIGN_LEFT | wxGROW | wxALL, borderSize);

    DoInitControls();

    assert(m_Contents->GetSizer() != 0); // Derived classes have to create a sizer for m_Contents
    m_Contents->SetMinClientSize(m_Contents->GetSizer()->GetMinSize());

    SetSizer(m_SzTop);
    Fit();
}

void c_ScrollableDialog::RefreshContents()
{
    // As of wxWidgets 3.0.2, sometimes some child controls remain unrefreshed (graphically corrupted), so refresh everything
    m_Contents->Refresh(false);
}

void c_ScrollableDialog::OnContentsResized(wxSizeEvent &event)
{
    RefreshContents();
}

void c_ScrollableDialog::OnContentsScrolled(wxScrollWinEvent &event)
{
    RefreshContents();
    event.Skip();
}
