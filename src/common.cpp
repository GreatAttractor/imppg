#include <wx/defs.h> // For some reason, this is needed before display.h, otherwise there are a lot of WXDLLIMPEXP_FWD_CORE undefined errors
#include <wx/display.h>
#include <wx/filename.h>
#include <wx/stdpaths.h>
#include "common.h"

/// Checks if a window is visible on any display; if not, sets its size and position to default
void FixWindowPosition(wxWindow &wnd)
{
    // The program could have been previously launched on multi-monitor setup
    // and the window moved to one of monitors which is no longer connected.
    // Detect it and set default position if neccessary.
    if (wxDisplay::GetFromWindow(&wnd) == wxNOT_FOUND)
    {
        wnd.SetPosition(wxPoint(0, 0)); // Using wxDefaultPosition does not work under Windows
        wnd.SetSize(wxDefaultSize);
    }
}

/// Loads a bitmap from the "images" subdirectory, optionally scaling it
wxBitmap LoadBitmap(wxString name, bool scale, wxSize scaledSize)
{
    wxFileName fName = wxFileName(wxStandardPaths::Get().GetExecutablePath());
    fName.AppendDir("images");
    fName.SetName(name);
    fName.SetExt("png");

    wxBitmap result(fName.GetFullPath(), wxBITMAP_TYPE_ANY);
    if (!result.IsOk())
        result = wxBitmap(16, 16); //TODO: show some warning/working path suggestion message
    else if (scale)
    {
        result = wxBitmap(result.ConvertToImage().Scale(scaledSize.GetWidth(), scaledSize.GetHeight(), wxIMAGE_QUALITY_BICUBIC));
    }

    return result; // Return by value; it's fast, because wxBitmap's copy constructor uses reference counting
}
