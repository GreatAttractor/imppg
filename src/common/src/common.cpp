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
    Common utils implementation.
*/

#include "common/common.h"
#include "common/dirs.h"
#include "image/image.h"

#include <cfloat>
#include <cstdlib>
#include <wx/defs.h> // For some reason, this is needed before display.h, otherwise there are a lot of WXDLLIMPEXP_FWD_CORE undefined errors
#include <wx/display.h>
#include <wx/settings.h>

/// Checks if a window is visible on any display; if not, sets its size and position to default
void FixWindowPosition(wxWindow& wnd)
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

/// Loads a bitmap from the "images" subdirectory, optionally scaling it.
/// In dark mode the RGB channels are inverted (alpha preserved) so that the
/// dark-on-transparent source icons remain visible on a dark toolbar.
wxBitmap LoadBitmap(wxString name, std::optional<wxSize> scaledSize)
{
    wxFileName fName = GetImagesDirectory();
    fName.SetName(name);
    fName.SetExt("png");

    wxImage img(fName.GetFullPath(), wxBITMAP_TYPE_ANY);
    if (!img.IsOk())
        return wxBitmap(16, 16); //TODO: show some warning/working path suggestion message

    if (wxSystemSettings::GetAppearance().IsDark())
    {
        unsigned char* rgb = img.GetData();
        const int npx = img.GetWidth() * img.GetHeight();
        for (int i = 0; i < npx * 3; ++i)
            rgb[i] = static_cast<unsigned char>(255 - rgb[i]);
    }

    if (scaledSize.has_value())
    {
        img = img.Scale(
            scaledSize->GetWidth(),
            scaledSize->GetHeight(),
            wxIMAGE_QUALITY_BICUBIC
        );
    }

    return wxBitmap(img);
}

Histogram DetermineHistogram(const c_Image& img, const wxRect& selection)
{
    constexpr int NUM_HISTOGRAM_BINS = 1024;

    IMPPG_ASSERT(
        img.GetPixelFormat() == PixelFormat::PIX_MONO32F ||
        img.GetPixelFormat() == PixelFormat::PIX_RGB32F
    );

    IMPPG_ASSERT(img.GetImageRect().Contains(selection));

    Histogram histogram{};

    histogram.values.insert(histogram.values.begin(), NUM_HISTOGRAM_BINS, 0);
    histogram.minValue = FLT_MAX;
    histogram.maxValue = -FLT_MAX;

    const std::size_t numChannels = NumChannels[static_cast<std::size_t>(img.GetPixelFormat())];

    for (int y = 0; y < selection.height; y++)
    {
        const float* row = img.GetRowAs<float>(selection.y + y) + selection.x;
        for (unsigned x = 0; x < selection.width * numChannels; x++)
        {
            if (row[x] < histogram.minValue)
                histogram.minValue = row[x];
            else if (row[x] > histogram.maxValue)
                histogram.maxValue = row[x];

            const unsigned hbin = static_cast<unsigned>(row[x] * (NUM_HISTOGRAM_BINS - 1));
            IMPPG_ASSERT(hbin < NUM_HISTOGRAM_BINS);
            histogram.values[hbin] += 1;
        }
    }

    histogram.maxCount = 0;
    for (unsigned i = 0; i < histogram.values.size(); i++)
        if (histogram.values[i] > histogram.maxCount)
            histogram.maxCount = histogram.values[i];

    return histogram;
}

Histogram DetermineHistogramFromChannels(const std::vector<c_Image>& channels, const wxRect& selection)
{
    constexpr int NUM_HISTOGRAM_BINS = 1024;

    const unsigned width = channels.at(0).GetWidth();
    IMPPG_ASSERT(channels.at(0).GetImageRect().Contains(selection));
    for (std::size_t i = 1; i < channels.size(); ++i)
    {
        IMPPG_ASSERT(channels[i].GetWidth() == width && channels[i].GetImageRect().Contains(selection));
    }

    Histogram histogram{};

    histogram.values.insert(histogram.values.begin(), NUM_HISTOGRAM_BINS, 0);
    histogram.minValue = FLT_MAX;
    histogram.maxValue = -FLT_MAX;

    for (const c_Image& channel: channels)
    {
        for (int y = 0; y < selection.height; y++)
        {
            const float* row = channel.GetRowAs<float>(selection.y + y) + selection.x;
            for (int x = 0; x < selection.width; x++)
            {
                if (row[x] < histogram.minValue)
                    histogram.minValue = row[x];
                else if (row[x] > histogram.maxValue)
                    histogram.maxValue = row[x];

                const unsigned hbin = static_cast<unsigned>(row[x] * (NUM_HISTOGRAM_BINS - 1));
                IMPPG_ASSERT(hbin < NUM_HISTOGRAM_BINS);
                histogram.values[hbin] += 1;
            }
        }
    }

    histogram.maxCount = 0;
    for (unsigned i = 0; i < histogram.values.size(); i++)
    {
        if (histogram.values[i] > histogram.maxCount)
        {
            histogram.maxCount = histogram.values[i];
        }
    }

    return histogram;
}

wxString GetBackEndText(BackEnd backEnd)
{
    switch (backEnd)
    {
    case BackEnd::CPU_AND_BITMAPS: return _("CPU + bitmaps");
    case BackEnd::GPU_OPENGL: return _("GPU (OpenGL)");

    default: IMPPG_ABORT();
    }
}
