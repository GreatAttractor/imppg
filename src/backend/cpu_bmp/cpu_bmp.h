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
    CPU & bitmaps back end declaration.
*/

#ifndef IMPPG_CPU_BMP_HEADER
#define IMPPG_CPU_BMP_HEADER

#include <functional>
#include <optional>
#include <wx/bitmap.h>
#include <wx/event.h>
#include <wx/timer.h>

#include "common.h"
#include "scrolled_view.h"
#include "backend/backend.h"
#include "backend/cpu_bmp/cpu_bmp_proc.h"

namespace imppg::backend {

class c_CpuAndBitmaps: public IDisplayBackEnd
{
public:
    c_CpuAndBitmaps(c_ScrolledView& imgView);

    ~c_CpuAndBitmaps() override;

    void ImageViewScrolledOrResized(float zoomFactor) override;

    void ImageViewZoomChanged(float zoomFactor) override;

    void SetImage(c_Image&& img, std::optional<wxRect> newSelection) override;

    Histogram GetHistogram() override;

    void SetPhysicalSelectionGetter(std::function<wxRect()> getter) override { m_PhysSelectionGetter = getter; }

    void SetScaledLogicalSelectionGetter(std::function<wxRect()> getter) override { m_ScaledLogicalSelectionGetter = getter; }

    void SetProcessingCompletedHandler(std::function<void(CompletionStatus)> handler) override { m_OnProcessingCompleted = handler; }

    void NewSelection(const wxRect& selection) override;

    void RefreshRect(const wxRect& rect) override;

    void NewProcessingSettings(const ProcessingSettings& procSettings) override;

    void LRSettingsChanged(const ProcessingSettings& procSettings) override;

    void UnshMaskSettingsChanged(const ProcessingSettings& procSettings) override;

    void ToneCurveChanged(const ProcessingSettings& procSettings) override;

    void SetScalingMethod(ScalingMethod scalingMethod) override;

    const std::optional<c_Image>& GetImage() const override { return m_Img; }

    void SetProgressTextHandler(std::function<void(wxString)> handler) override { m_Processor.SetProgressTextHandler(handler); }

    c_Image GetProcessedSelection() override;

    bool ProcessingInProgress() override;

    void AbortProcessing() override;


private:

    c_CpuAndBitmapsProcessing m_Processor;

    c_ScrolledView& m_ImgView;

    std::optional<c_Image> m_Img;

    std::optional<wxBitmap> m_ImgBmp; ///< Bitmap which wraps `m_Img` for displaying on `m_ImgView`.

    float m_ZoomFactor{ZOOM_NONE};

    float m_NewZoomFactor{ZOOM_NONE};

    std::optional<wxBitmap> m_BmpScaled; ///< Currently visible scaled fragment (or whole) of `m_ImgBmp`.

    wxRect m_ScaledArea; ///< Area within `m_ImgBmp` represented by `m_BmpScaled`.

    wxRect m_Selection; ///< Image fragment selected for processing (in logical image coords).

    /// Provides selection in physical image view coords.
    std::function<wxRect()> m_PhysSelectionGetter;

    std::function<wxRect()> m_ScaledLogicalSelectionGetter;

    std::function<void(CompletionStatus)> m_OnProcessingCompleted;

    class c_ScalingTimer: public wxTimer
    {
        std::function<void()> m_Handler;
    public:
        void SetHandler(std::function<void()> handler) { m_Handler = handler; }
        void Notify() override { m_Handler(); }
    } m_ScalingTimer;

    ScalingMethod m_ScalingMethod{ScalingMethod::LINEAR};

    void OnPaint(wxPaintEvent& event);

    void CreateScaledPreview(float zoomFactor);

    void UpdateSelectionAfterProcessing();
};

} // namespace imppg::backend

#endif // IMPPG_CPU_BMP_HEADER
