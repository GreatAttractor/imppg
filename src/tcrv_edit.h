/*
ImPPG (Image Post-Processor) - common operations for astronomical stacks and other images
Copyright (C) 201, 2016 Filip Szczerek <ga.software@yahoo.com>

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
    Tone curve editor header.
*/

#ifndef IMPGG_TONE_CURVE_EDITOR_H
#define IMPGG_TONE_CURVE_EDITOR_H

#include <wx/datetime.h>
#include <wx/dc.h>
#include <wx/event.h>
#include <wx/gdicmn.h>
#include <wx/panel.h>
#include <wx/tglbtn.h>
#include <wx/timer.h>
#include <wx/checkbox.h>
#include <wx/spinctrl.h>

#include "daction.h"
#include "common/tcrv.h"

// Event sent to parent every time the curve is changed
wxDECLARE_EVENT(EVT_TONE_CURVE, wxCommandEvent);

class c_ToneCurveEditor: public wxPanel, IDelayedAction
{
private:
    enum class CanAddPointSide { Left, Right };

    // Event handlers ----------------------------
    void OnPaintCurveArea(wxPaintEvent& event);
    void OnCurveAreaLeftDown(wxMouseEvent& event);
    void OnCurveAreaLeftUp(wxMouseEvent& event);
    void OnCurveAreaRightDown(wxMouseEvent& event);
    void OnCurveAreaMouseMove(wxMouseEvent& event);
    void OnReset(wxCommandEvent& event);
    void OnToggleSmooth(wxCommandEvent& event);
    void OnMouseCaptureLost(wxMouseCaptureLostEvent& event);
    void OnToggleLogHist(wxCommandEvent& event);
    void OnToggleGammaMode(wxCommandEvent& event);
    void OnChangeGamma(wxSpinDoubleEvent& event);
    void OnInvert(wxCommandEvent& event);
    void OnStretch(wxCommandEvent& event);
    //--------------------------------------------

    /// Returns 'true' if distance between point [pointIdx] and (x,y) is below MIN_GRAB_DIST
    bool IsPointDistBelowThreshold(float x, float y, int pointIdx);

    void SetToolTips();
    /// Converts pixel coords to curve logical coords (minx<=logx<=maxx, 0<=logy<=1)
    void GetCurveLogicalCoords(int x, int y, float minx, float maxx, float* logx, float* logy);

    void DrawCurve(
        wxDC& dc,
        const wxRect& r ///< Represents the drawing area
        );

    void MarkCurvePoints(
        wxDC& dc,
        const wxRect& r ///< Represents the drawing area
        );

    void DrawHistogram(
        wxDC& dc,
        const wxRect& r ///< Represents the drawing area
        );

    void DeactivateGammaMode();

    /// Sends EVT_TONE_CURVE to parent, but not more often than the delay specified in the constructor
    void DoPerformAction(void* param = nullptr) override;

    /// Sends EVT_TONE_CURVE event to parent, but no more frequently than each 'm_UpdateEvtDelay' milliseconds
    void SignalToneCurveUpdate();

    float CorrectLogicalXPosition(std::size_t pointIdx, float x);

    std::optional<CanAddPointSide> CanAddPoint(float x);

    wxPanel* m_CurveArea{nullptr};
    c_ToneCurve* m_Curve{nullptr};
    struct
    {
        bool dragging;
        float minx, maxx; ///< The allowed X range to drag the current point within
        std::size_t draggedPointIdx;
    } m_MouseOps;

    Histogram m_Histogram{};

    /// 'True' if histogram is displayed using logarithmic scale (values only)
    bool m_LogarithmicHistogram;

    /// Number of curve and histogram segments to draw
    size_t m_NumDrawSegments;

    wxSpinCtrlDouble* m_GammaCtrl{nullptr};
    wxCheckBox* m_GammaToggleCtrl{nullptr};
    wxCheckBox* m_SmoothCtrl{nullptr};
    wxToggleButton* m_LogarithmicCtrl{nullptr};

public:
    c_ToneCurveEditor(
        wxWindow* parent,
        c_ToneCurve* curve,
        int id = wxID_ANY,
        unsigned updateEvtDelay = 0, ///< Delay in milliseconds between sending subsequent EVT_TONE_CURVE events
        bool logarithmicHistogram = false ///< If true, histogram is displayed in logarithmic scale
    );

    c_ToneCurve* GetToneCurve() { return m_Curve; }

    /// Sets the tone curve and updates the display; does not generate an EVT_TONE_CURVE event
    void SetToneCurve(c_ToneCurve* curve);

    /// Updates the histogram (creates an internal copy)
    void SetHistogram(const Histogram& histogram);

    bool IsHistogramLogarithmic() { return m_LogarithmicHistogram; }
    void SetHistogramLogarithmic(bool value);

    DECLARE_EVENT_TABLE()
};

#endif
