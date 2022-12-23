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
#include <optional>
#include <wx/aui/framemanager.h>
#include <wx/bitmap.h>
#include <wx/dc.h>
#include <wx/event.h>
#include <wx/frame.h>
#include <wx/intl.h>
#include <wx/panel.h>
#include <wx/scrolwin.h>
#include <wx/stattext.h>
#include <wx/timer.h>

#include "backend/backend.h"
#include "common/scrolled_view.h"
#include "image/image.h"
#include "num_ctrl.h"
#include "common/proc_settings.h"
#include "tcrv_edit.h"
#include "common/tcrv.h"

class c_MainWindow: public wxFrame
{
    // Event handlers ----------
    void OnClose(wxCloseEvent& event);
    void OnCommandEvent(wxCommandEvent& event);
    void OnImageViewMouseDragStart(wxMouseEvent& event);
    void OnImageViewMouseMove(wxMouseEvent& event);
    void OnImageViewMouseDragEnd(wxMouseEvent& event);
    void OnImageViewSize(wxSizeEvent& event);
    void OnImageViewDragScrollStart(wxMouseEvent& event);
    void OnImageViewDragScrollEnd(wxMouseEvent& event);
    void OnOpenFile(wxCommandEvent& event);
    void OnToneCurveChanged(wxCommandEvent& event);
    void OnCloseToneCurveEditorWindow(wxCloseEvent& event);
    void OnLucyRichardsonIters(wxSpinEvent& event);
    void OnImageViewMouseCaptureLost(wxMouseCaptureLostEvent& event);
    void OnAuiPaneClose(wxAuiManagerEvent& event);
    void OnImageViewMouseWheel(wxMouseEvent& event);
    void OnProcessingPanelScrolled(wxScrollWinEvent& event);
    void OnSettingsFile(wxCommandEvent& event);
    void OnUnsharpMaskingControlChanged(wxCommandEvent& event);
    //--------------------------

    void OnNewSelection(
            wxRect newSelection ///< Logical coordinates in the image
    );
    void OnUpdateLucyRichardsonSettings();
    void InitToolbar();
    void InitStatusBar();
    void InitControls();
    void InitMenu();
    /// Returns a panel containing the processing controls
    wxWindow* CreateProcessingControlsPanel();
    /// Marks the selection's outline (using physical coords) on the specified DC
    void MarkSelection(const wxRect& selection, wxDC& dc);
    void SetActionText(wxString text); ///< Sets text in the first field of the status bar
    bool SharpeningEnabled(); ///< Returns 'true' if sharpening settings have impact on the image
    bool UnshMaskingEnabled(); ///< Returns 'true' if unsharp masking settings have impact on the image
    bool ToneCurveEnabled(); ///< Returns 'true' if tone curve has impact on the image (i.e. it is not the identity map)
    wxPanel* CreateLucyRichardsonControlsPanel(wxWindow* parent);
    wxStaticBoxSizer* CreateUnsharpMaskingControls(wxWindow* parent);
    void OnUpdateUnsharpMaskingSettings();
    /// Updates state of menu items and toolbar buttons responsible for toggling the processing panel and tone curve editor
    void UpdateToggleControlsState();
    /// Returns the ratio of 'm_ImgView' to the size of 'imgSize', assuming uniform scaling in "touch from inside" fashion
    float GetViewToImgRatio() const;
    void OnSaveFile();
    void OpenFile(wxFileName path, bool resetSelection);
    /// Displays the UI language selection dialog
    void SelectLanguage();
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
    void SetUnsharpMaskingControlsVisibility();
    /// Adds or moves 'settingsFile' to the beginning of the most recently used list
    /** Also updates m_MruSettingsIdx. */
    void SetAsMRU(wxString settingsFile);
    void LoadSettingsFromFile(wxString settingsFile, bool moveToMruListStart);
    void IndicateSettingsModified();
    wxRect GetPhysicalSelection() const; ///< Returns current selection in physical `m_ImageView` coords.
#if ENABLE_SCRIPTING
    void RunScript();
#endif

    template<typename T>
    void InitializeBackEnd(std::unique_ptr<T> backEnd, std::optional<c_Image> img);

    wxAuiManager* m_AuiMgr{nullptr};
    c_ScrolledView* m_ImageView{nullptr}; ///< Displays 'm_ImgBmp' or 'm_ImgBmpScaled' (i.e. the current image)
    wxFrame m_ToneCurveEditorWindow;
    wxString m_LastChosenSettingsFileName;
    wxStaticText* m_LastChosenSettings; ///< Shows the last chosen settings file name in toolbar

    struct
    {
        c_NumericalCtrl* lrSigma{nullptr};
        wxSpinCtrl* lrIters{nullptr};
        wxCheckBox* lrDeriging{nullptr};
        wxCheckBox* unshAdaptive{nullptr};

        c_NumericalCtrl* unshSigma{nullptr};
        c_NumericalCtrl* unshAmountMin{nullptr};
        c_NumericalCtrl* unshAmountMax{nullptr};
        c_NumericalCtrl* unshThreshold{nullptr};
        c_NumericalCtrl* unshWidth{nullptr};

        c_ToneCurveEditor* tcrvEditor{nullptr};

    } m_Ctrls;

    bool m_FitImageInWindow; ///< If 'true', image view is scaled to fit the program window

    /// Index of the most recently used item from Configuration::GetMruSettings().
    int m_MruSettingsIdx{wxNOT_FOUND};

    bool m_ImageLoaded{false}; ///< Becomes true after first successful image load.

    /// Current image, processing settings and processing steps' results.
    struct
    {
        wxString inputFilePath;

        ScalingMethod scalingMethod;

        bool m_FileSaveScheduled;

        /// Current selection within the image (for processing preview); expressed in logical coords (i.e. scrolling not included).
        wxRect selection;

        unsigned imgWidth{0};
        unsigned imgHeight{0};

        /// Contains `selection` expressed in logical coordinates of `m_ImageView` when view.zoomFactor <> 1.
        wxRect scaledSelection;

        struct
        {
            float zoomFactor;
            bool zoomFactorChanged; ///< If 'true', the next Refresh() after scaling needs to erase background; the flag is then cleared

            /// Triggers the image view's bitmap scaling some time after the last scrolling, zooming, etc.
            wxTimer scalingTimer;
        } view;

        ProcessingSettings processing;

    } m_CurrentSettings;

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
        wxRect GetSelection(wxRect limit) const
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

    std::unique_ptr<imppg::backend::IDisplayBackEnd> m_BackEnd;

public:
    c_MainWindow();
    ~c_MainWindow() override;


    DECLARE_EVENT_TABLE()
};

#endif // MYAPP_MAIN_H
