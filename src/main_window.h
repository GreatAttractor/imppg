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
    Main window class header.
*/

#ifndef MYAPP_MAIN_H
#define MYAPP_MAIN_H

#include <algorithm>
#include <cstdlib>
#include <memory>
#include <wx/frame.h>
#include <wx/aui/framemanager.h>
#include <wx/event.h>
#include <wx/panel.h>
#include <wx/bitmap.h>
#include <wx/scrolwin.h>
#include <wx/dc.h>
#include <wx/stattext.h>
#include <wx/timer.h>
#include <wx/intl.h>
#include "image.h"
#include "worker.h"
#include "tcrv.h"

enum class ScalingMethod
{
    NEAREST = 0,
    LINEAR,
    CUBIC,

    NUM_METHODS // This has to be the last entry
};

class c_MainWindow: public wxFrame
{
    // Event handlers ----------
    void OnClose(wxCloseEvent &event);
    void OnPaintImageArea(wxPaintEvent &event);
    void OnCommandEvent(wxCommandEvent &event);
    void OnImageViewMouseDragStart(wxMouseEvent &event);
    void OnImageViewMouseMove(wxMouseEvent &event);
    void OnImageViewMouseDragEnd(wxMouseEvent &event);
    void OnImageViewSize(wxSizeEvent &event);
    void OnImageViewDragScrollStart(wxMouseEvent &event);
    void OnImageViewDragScrollEnd(wxMouseEvent &event);
    void OnOpenFile(wxCommandEvent &event);
    void OnThreadEvent(wxThreadEvent &event);
    void OnToneCurveChanged(wxCommandEvent &event);
    void OnCloseToneCurveEditorWindow(wxCloseEvent &event);
    void OnLucyRichardsonIters(wxSpinEvent &event);
    void OnImageViewMouseCaptureLost(wxMouseCaptureLostEvent &event);
    void OnAuiPaneClose(wxAuiManagerEvent &event);
    void OnTimer(wxTimerEvent &event);
    void OnImageViewScroll(wxScrollWinEvent &event);
    void OnImageViewMouseWheel(wxMouseEvent &event);
    void OnProcessingPanelScrolled(wxScrollWinEvent &event);
    void OnSettingsFile(wxCommandEvent &event);
    //--------------------------

    void OnNewSelection(
            wxRect newSelection ///< Logical coordinates in the image
    );
    void OnUpdateLucyRichardsonSettings();
    void InitToolbar();
    void InitStatusBar();
    void InitControls();
    void InitMenu();
    /// Converts the specified fragment of 'src' to a 24-bit RGB bitmap
    wxBitmap *ImageToRgbBitmap(const c_Image &src, int x0, int y0, int width, int height);
    /// Returns a panel containing the processing controls
    wxWindow *CreateProcessingControlsPanel();
    /// Marks the selection's outline (using physical coords) on the specified DC
    void MarkSelection(wxRect &selection, wxDC &dc);
    /// Aborts processing and schedules new processing to start ASAP (as soon as 'm_Processing.worker' is not running)
    void ScheduleProcessing(ProcessingRequest request);
    void StartProcessing(); ///< Creates and starts a background processing thread
    void UpdateSelectionAfterProcessing();
    bool IsProcessingInProgress(); ///< Returns 'true' if the processing thread is running
    void SetActionText(wxString text); ///< Sets text in the first field of the status bar
    bool SharpeningEnabled(); ///< Returns 'true' if sharpening settings have impact on the image
    bool UnshMaskingEnabled(); ///< Returns 'true' if unsharp masking settings have impact on the image
    bool ToneCurveEnabled(); ///< Returns 'true' if tone curve has impact on the image (i.e. it is not the identity map)
    void OnProcessingStepCompleted(CompletionStatus status);
    /// Determines histogram of the specified area of an image
    void DetermineHistogram(const c_Image &img, const wxRect &selection, Histogram_t &histogram);
    wxPanel *CreateLucyRichardsonControlsPanel(wxWindow *parent);
    wxStaticBoxSizer *CreateUnsharpMaskingControls(wxWindow *parent);
    void OnUpdateUnsharpMaskingSettings();
    /// Updates state of menu items and toolbar buttons responsible for toggling the processing panel and tone curve editor
    void UpdateToggleControlsState();
    /// Returns the ratio of 'm_ImgView' to the size of 'imgSize', assuming uniform scaling in "touch from inside" fashion
    float GetViewToImgRatio() const;
    void OnSaveFile();
    void OpenFile(wxFileName path, bool resetSelection);
    /// Displays the UI language selection dialog
    void SelectLanguage();
    void ScheduleScalingRequest();
    /// Must be called to finalize a zoom change
    void OnZoomChanged(
        wxPoint zoomingCenter ///< Point (physical coordinates) in m_ImageView to be kept stationary
        );
    void UpdateWindowTitle();
    void ChangeZoom(
            float newZoomFactor,
            wxPoint zoomingCenter ///< Point (physical coordinates) in m_ImageView to be kept stationary
            );
    float CalcZoomIn(float currentZoom);
    float CalcZoomOut(float currentZoom);
    void CreateScaledPreview(bool eraseBackground);
    void SetUnsharpMaskingControlsVisibility();
    /// Adds or moves 'settingsFile' to the beginning of the most recently used list
    /** Also updates m_MruSettingsIdx. */
    void SetAsMRU(wxString settingsFile);
    void LoadSettingsFromFile(wxString settingsFile, bool moveToMruListStart);
    void IndicateSettingsModified();

    wxAuiManager m_AuiMgr;
    wxScrolledCanvas m_ImageView; ///< Displays 'm_ImgBmp' or 'm_ImgBmpScaled' (i.e. the current image)
    wxFrame m_ToneCurveEditorWindow;
    wxString m_LastChosenSettingsFileName;
    wxStaticText *m_LastChosenSettings; ///< Shows the last chosen settings file name in toolbar

    bool m_FitImageInWindow; ///< If 'true', image view is scaled to fit the program window

    /// Index of the most recently used item from Configuration::GetMruSettings(); on program start equals wxNOT_FOUND
    int m_MruSettingsIdx;

    /// Wraps an image and all associated processing settings
    typedef struct
    {
        wxString inputFilePath;

        c_Image m_Img; ///< Image (mono) in floating-point format
        std::unique_ptr<wxBitmap> m_ImgBmp; ///< Bitmap which wraps 'm_Img' for displaying on 'm_ImageView'

        ScalingMethod scalingMethod;

        bool m_FileSaveScheduled;

        /// Current selection within the image (for processing preview); expressed in logical coords (i.e. scrolling not included)
        wxRect selection;

        /// 'selection' expressed in logical coordinates of 'm_ImageView' when view.zoomFactor <> 1
        wxRect scaledSelection;

        struct
        {
            float zoomFactor;
            std::unique_ptr<wxBitmap> bmpScaled; ///< Currently visible scaled fragment (or whole) of 'm_ImgBmp'
            wxRect scaledArea; ///< Area within 'm_ImgBmp' represented by 'bmpScaled'

            bool zoomFactorChanged; ///< If 'true', the next Refresh() after scaling needs to erase background; the flag is then cleared

            /// Triggers the image view's bitmap scaling some time after the last scrolling, zooming, etc.
            wxTimer scalingTimer;
        } view;

        /// Normalization is performed prior to all other processing steps
        struct
        {
            bool enabled;
            float min, max;
        } normalization;

        /// Incremental results of processing of the current selection
        /** Must not be accessed when the relevant background thread is running. */
        struct
        {
            /// Results of sharpening
            struct
            {
                c_Image img;
                bool valid; ///< 'true' if the last sharpening request completed
            } sharpening;

            /// Results of sharpening and unsharp masking
            struct
            {
                c_Image img;
                bool valid; ///< 'true' if the last unsharp masking request completed
            } unsharpMasking;

            /// Results of sharpening, unsharp masking and applying of tone curve
            struct
            {
                c_Image img;
                bool valid; ///< 'true' if the last tone curve application request completed
                bool preciseValuesApplied; ///< 'true' if precise values of tone curve have been applied; happens only when saving output file
            } toneCurve;

        } output;

        c_ToneCurve toneCurve;

        struct
        {
            float sigma; ///< Lucy-Richardson deconvolution kernel sigma
            int iterations; ///< Number of Lucy-Richardson deconvolution iterations
            struct
            {
                bool enabled; ///< Experimantal; enables deringing along edges of overexposed areas (see c_LucyRichardsonThread::DoWork())
            } deringing;
        } LucyRichardson;

        struct
        {
            // See comments in c_UnsharpMaskingThread::DoWork() for details

            bool adaptive; ///If true, adaptive unsharp masking is used
            float sigma; ///< Gaussian kernel sigma
            float amountMin; ///< amount (weight) of the unsharped layer; <1.0 blurs, >1.0 sharpens; if adaptive=true, used as the min amount
            float amountMax; ///< max amount
            float threshold; ///< Threshold of input image brightness where the min-max amount transition occurs
            float width; ///< Width of the transition interval
        } UnsharpMasking;
    } ProcessingSettings_t;

    /// Processing settings and results for the currently active image
    std::shared_ptr<ProcessingSettings_t> m_CurrentSettings;

    struct
    {
        bool dragging; ///< True when dragging with left button down
        bool prevSelectionBordersErased;

        wxPoint dragStart; ///< Logical coordinates in m_ImgBmp
        wxPoint dragEnd; ///< Logical coordinates in m_ImgBmp

        /// Screen coordinates of drag position (relative to m_ImageView's origin (unscrolled))
        struct
        {
            wxPoint start, end;
        } View;

        /// Returns the selection made with mouse (logical coords in m_ImgBmp), limited to the specified rectangle
        wxRect GetSelection(wxRect limit)
        {
            wxPoint topLeft(std::min(dragStart.x, dragEnd.x),
                std::min(dragStart.y, dragEnd.y));

            wxRect result = wxRect(topLeft.x, topLeft.y,
                std::abs(dragEnd.x - dragStart.x) + 1,
                std::abs(dragEnd.y - dragStart.y) + 1);

            if (result.x < limit.x)
            {
                result.width -= limit.x - result.x;
                result.x = limit.x;
            }
            else if (result.x >= limit.x + limit.width)
                result.x = limit.x + limit.width - 1;

            if (result.y < limit.y)
            {
                result.height -= limit.y - result.y;
                result.y = limit.y;
            }
            else if (result.y >= limit.y + limit.height)
                result.y = limit.y + limit.height - 1;

            if (result.x + result.width > limit.x + limit.width)
                result.width = limit.x + limit.width - result.x;

            if (result.y + result.height > limit.y + limit.height)
                result.height = limit.y + limit.height - result.y;

            return result;
        }

        /// Variables for tracking drag-scrolling of the image area with middle mouse button
        struct
        {
            bool dragging; ///< True when dragging with middle mouse button down
            wxPoint start; ///< Drag start point
            wxPoint startScrollPos;
        } dragScroll;
    } m_MouseOps;

    /// Background processing-related variables
    struct
    {
        c_WorkerThread *worker;
        wxCriticalSection guard; ///< Guards access to 'worker'
        /// Identifier increased by 1 after each creation of a new thread
        unsigned currentThreadId;
        /// If 'true', tone curve is applied using its precise values. Otherwise, the curve's LUT is used.
        /** Set to 'false' when user is just editing and adjusting settings. Set to 'true' when an output file is saved. */
        bool usePreciseTCurveVals;

        /// Currently scheduled processing request.
        /** When a request of type 'n' (from the enum below) is generated (by user actions in the GUI),
        all processing steps designated by values >=n are performed.

        For example, if the user changes sharpening settings (e.g. the L-R kernel sigma),
        the currently selected area is sharpened, then unsharp masking is performed,
        and finally the tone curve applied. If, however, the user changes only a control point
        in the tone curve editor, only the (updated) tone curve is applied (to the results
        of the last performed unsharp masking). */
        ProcessingRequest processingRequest;

        /// If 'true', processing has been scheduled to start ASAP (as soon as 'm_Processing.worker' is not running)
        bool processingScheduled;
    } m_Processing;

public:
    c_MainWindow();
    virtual ~c_MainWindow();


    DECLARE_EVENT_TABLE()
};

#endif // MYAPP_MAIN_H
