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
    Scrollable dialog class header.
*/

#ifndef IMPPG_SCROLLABLE_DIALOG_HEADER
#define IMPPG_SCROLLABLE_DIALOG_HEADER

#include <wx/dialog.h>
#include <wx/event.h>
#include <wx/scrolwin.h>
#include <wx/sizer.h>
#include <wx/string.h>

class c_ScrollableDialog: public wxDialog
{
public:
    c_ScrollableDialog(wxWindow* parent, wxWindowID id, wxString title);
    virtual ~c_ScrollableDialog();

private:
    /** Container that automatically enables scrolling of its contents if its size becomes small enough;
    most controls (except perhaps "OK"/"Cancel" buttons) should be its children. Derived classes have to create
    a sizer first, assign it to m_Contents and then add child controls to this sizer. */
    wxScrolledWindow* m_Contents{nullptr};

    /** Top-level sizer, which contains 'm_Contents'. Derived classes can add more items to this sizer
    (e.g. a sizer with "OK"/"Cancel" buttons), though they will not be automatically scrollable. */
    wxBoxSizer* m_SzTop;

    void OnContentsScrolled(wxScrollWinEvent& event);
    void OnContentsResized(wxSizeEvent& event);

    void RefreshContents();

protected:
    wxBoxSizer* GetTopSizer() const { return m_SzTop; }
    wxScrolledWindow* GetContainer() const { return m_Contents; }
    void AssignContainerSizer(wxSizer* sizer) { m_Contents->SetSizer(sizer); }

    /// Derived classes must call this method when creating the dialog
    void InitControls(int borderSize);

    /// Derived class must create controls in this method
    virtual void DoInitControls() = 0;
};

#endif // IMPPG_SCROLLABLE_DIALOG_HEADER
