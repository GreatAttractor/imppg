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
    Tone curve editor implementation.
*/

#include <math.h>
#include <algorithm>
#include <wx/dcclient.h>
#include <wx/dcmemory.h>
#include <wx/pen.h>
#include <wx/brush.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/bitmap.h>
#include <wx/tglbtn.h>
#include <wx/spinctrl.h>
#include "tcrv_edit.h"
#include "cursors.h"
#include "ctrl_ids.h"
#include "common.h"

// Event sent to parent every time the curve is changed
wxDEFINE_EVENT(EVT_TONE_CURVE, wxCommandEvent);

BEGIN_EVENT_TABLE(c_ToneCurveEditor, wxWindow)
    EVT_BUTTON(ID_Reset, c_ToneCurveEditor::OnReset)
    EVT_CHECKBOX(ID_Smooth, c_ToneCurveEditor::OnToggleSmooth)
    EVT_TOGGLEBUTTON(ID_Logarithmic, c_ToneCurveEditor::OnToggleLogHist)
    EVT_SPINCTRLDOUBLE(ID_GammaCtrl, c_ToneCurveEditor::OnChangeGamma)
    EVT_CHECKBOX(ID_GammaCheckBox, c_ToneCurveEditor::OnToggleGammaMode)
    EVT_BUTTON(ID_Invert, c_ToneCurveEditor::OnInvert)
    EVT_BUTTON(ID_Stretch, c_ToneCurveEditor::OnStretch)
END_EVENT_TABLE()

const int BORDER = 5; ///< Border size (in pixels) between controls

const wxColour CL_CURVE_BACKGROUND = *wxWHITE;
const wxColour CL_CURVE_POINT = wxColour(0xAAAAAA);
const wxColour CL_SELECTED_CURVE_POINT = *wxRED;
const wxColour CL_HISTOGRAM = wxColour(0xEEEEEE);

/// Minimum grabbing distance of mouse cursor from a curve control point.
/** Distance is expressed as a percentage of max(width, height) of the curve view area. */
const int MIN_GRAB_DIST = 5;

void c_ToneCurveEditor::OnStretch(wxCommandEvent &event)
{
    if (m_Histogram.values.size() > 0)
    {
        m_Curve->Stretch(m_Histogram.minValue, m_Histogram.maxValue);
        m_CurveArea->Refresh(false);
        DelayedAction();
    }
}

void c_ToneCurveEditor::OnChangeGamma(wxSpinDoubleEvent &event)
{
    if (m_Curve->IsGammaMode())
    {
        m_Curve->SetGamma(((wxSpinCtrlDouble *)FindWindowById(ID_GammaCtrl, this))->GetValue());
        m_CurveArea->Refresh(false);
        DelayedAction();
    }
}

void c_ToneCurveEditor::DeactivateGammaMode()
{
    m_Curve->SetGammaMode(false);
    ((wxCheckBox *)FindWindowById(ID_GammaCheckBox, this))->SetValue(false);
}

void c_ToneCurveEditor::OnToggleGammaMode(wxCommandEvent &event)
{
    m_Curve->SetGammaMode(((wxCheckBox *)FindWindowById(ID_GammaCheckBox, this))->GetValue());
    m_Curve->SetGamma(((wxSpinCtrlDouble *)FindWindowById(ID_GammaCtrl, this))->GetValue());
    m_CurveArea->Refresh(false);
    DelayedAction();
}

void c_ToneCurveEditor::OnToggleLogHist(wxCommandEvent &event)
{
    m_LogarithmicHistogram = !m_LogarithmicHistogram;
    Refresh(false);
}

/// Updates the histogram (creates an internal copy)
void c_ToneCurveEditor::SetHistogram(Histogram_t &histogram)
{
    m_Histogram = histogram;
    m_CurveArea->Refresh(false);
}

void c_ToneCurveEditor::OnToggleSmooth(wxCommandEvent &event)
{
    m_Curve->SetSmooth((bool)event.GetInt());
    m_CurveArea->Refresh(false);
    DelayedAction();
}

void c_ToneCurveEditor::SetToneCurve(c_ToneCurve *curve)
{
    m_Curve = curve;
    ((wxCheckBox *)FindWindowById(ID_Smooth, this))->SetValue(m_Curve->GetSmooth());
    ((wxCheckBox *)FindWindowById(ID_GammaCheckBox, this))->SetValue(m_Curve->IsGammaMode());
    if (m_Curve->IsGammaMode())
        ((wxSpinCtrlDouble *)FindWindowById(ID_GammaCtrl, this))->SetValue(m_Curve->GetGamma());
    m_CurveArea->Refresh(false);
}

/// Converts pixel coords to curve logical coords (minx<=logx<=maxx, 0<=logy<=1)
void c_ToneCurveEditor::GetCurveLogicalCoords(int x, int y, float minx, float maxx, float *logx, float *logy)
{
    *logx = (float)x / m_CurveArea->GetClientRect().width;
    *logy = 1.0f - (float)y / m_CurveArea->GetClientRect().height;

    if (*logx < minx)
        *logx = minx;
    else if (*logx > maxx)
        *logx = maxx;

    if (*logy < 0)
        *logy = 0;
    else if (*logy > 1)
        *logy = 1;
}

void c_ToneCurveEditor::OnInvert(wxCommandEvent &event)
{
    m_Curve->Invert();
    m_CurveArea->Refresh(false);
    DelayedAction();
}

void c_ToneCurveEditor::OnReset(wxCommandEvent &event)
{
    m_Curve->Reset();
    ((wxCheckBox *)FindWindowById(ID_Smooth, this))->SetValue(m_Curve->GetSmooth());
    ((wxCheckBox *)FindWindowById(ID_GammaCheckBox, this))->SetValue(m_Curve->IsGammaMode());
    m_CurveArea->Refresh(false);
    DelayedAction();
}

void c_ToneCurveEditor::OnCurveAreaMouseMove(wxMouseEvent &event)
{
    // Event's logical coordinates within the curve area
    float x, y;

    if (!m_MouseOps.dragging)
    {
        GetCurveLogicalCoords(event.GetX(), event.GetY(), 0, 1, &x, &y);
        int pIdx = m_Curve->GetIdxOfClosestPoint(x, y);
        if (IsPointDistBelowThreshold(x, y, pIdx))
            m_CurveArea->SetCursor(wxCURSOR_SIZING);
        else
            m_CurveArea->SetCursor(*wxCROSS_CURSOR);
    }
    else if (m_MouseOps.dragging)
    {
        m_CurveArea->SetCursor(Cursors::crHandClosed);

        GetCurveLogicalCoords(event.GetX(), event.GetY(), m_MouseOps.minx, m_MouseOps.maxx, &x, &y);

        m_Curve->UpdatePoint(m_MouseOps.draggedPointIdx, x, y);
        m_CurveArea->Refresh(false);
        DelayedAction();
    }
}

void c_ToneCurveEditor::OnMouseCaptureLost(wxMouseCaptureLostEvent &event)
{
    m_MouseOps.draggedPointIdx = -1;
    m_MouseOps.dragging = false;
}

/// Returns 'true' if distance between point [pointIdx] and (x,y) is below MIN_GRAB_DIST
bool c_ToneCurveEditor::IsPointDistBelowThreshold(float x, float y, int pointIdx)
{
    return (SQR(m_Curve->GetPoint(pointIdx).x - x) +
            SQR(m_Curve->GetPoint(pointIdx).y - y)
            <= SQR(MIN_GRAB_DIST / 100.0f));
}

void c_ToneCurveEditor::OnCurveAreaRightDown(wxMouseEvent &event)
{
    // Event's logical coordinates within the curve area
    float x, y;
    GetCurveLogicalCoords(event.GetX(), event.GetY(), m_MouseOps.minx, m_MouseOps.maxx, &x, &y);

    int pIdx = m_Curve->GetIdxOfClosestPoint(x, y);

    if (IsPointDistBelowThreshold(x, y, pIdx))
    {
        m_Curve->RemovePoint(pIdx);
        Refresh(false);
        DelayedAction();
    }
}

void c_ToneCurveEditor::OnCurveAreaLeftDown(wxMouseEvent &event)
{
    m_MouseOps.minx = 0.0f;
    m_MouseOps.maxx = 1.0f;

    // Event's logical coordinates within the curve area
    float x, y;
    GetCurveLogicalCoords(event.GetX(), event.GetY(), m_MouseOps.minx, m_MouseOps.maxx, &x, &y);

    int pIdx = m_Curve->GetIdxOfClosestPoint(x, y);

    if (IsPointDistBelowThreshold(x, y, pIdx))
    {
        // Grab an existing point
        m_MouseOps.draggedPointIdx = pIdx;
    }
    else
    {
        // Create and grab a new point
        m_MouseOps.draggedPointIdx = m_Curve->AddPoint(x, y);
        m_CurveArea->SetCursor(wxCURSOR_SIZING);
        DeactivateGammaMode();
    }

    m_MouseOps.dragging = true;

    // Set 'minx' and 'maxx' appropriately so that
    // the point cannot be dragged before its
    // predecessor or after its successor
    if (m_MouseOps.draggedPointIdx == 0)
        m_MouseOps.minx = 0;
    else
        m_MouseOps.minx = m_Curve->GetPoint(m_MouseOps.draggedPointIdx - 1).x;

    if (m_MouseOps.draggedPointIdx == m_Curve->GetNumPoints() - 1)
        m_MouseOps.maxx = 1;
    else
        m_MouseOps.maxx = m_Curve->GetPoint(m_MouseOps.draggedPointIdx + 1).x;

    m_CurveArea->CaptureMouse();
    wxClientDC dc(m_CurveArea);
    MarkCurvePoints(dc, m_CurveArea->GetClientRect());
}

void c_ToneCurveEditor::OnCurveAreaLeftUp(wxMouseEvent &event)
{
    // Event's logical coordinates within the curve area
    float x, y;
    GetCurveLogicalCoords(event.GetX(), event.GetY(), m_MouseOps.minx, m_MouseOps.maxx, &x, &y);

    if (m_MouseOps.dragging)
    {
        m_Curve->UpdatePoint(m_MouseOps.draggedPointIdx, x, y);
        m_MouseOps.draggedPointIdx = -1;
        m_MouseOps.dragging = false;

        m_CurveArea->ReleaseMouse();
        m_CurveArea->SetCursor(*wxCROSS_CURSOR);
    }

    m_CurveArea->Refresh(false);
    DelayedAction();
}

void c_ToneCurveEditor::MarkCurvePoints(
    wxDC &dc,
    const wxRect &r ///< Represents the drawing area
 )
{
    for (int i = 0; i < m_Curve->GetNumPoints(); i++)
    {
        if (i != m_MouseOps.draggedPointIdx)
            dc.SetBrush(wxBrush(CL_CURVE_POINT));
        else
            dc.SetBrush(wxBrush(CL_SELECTED_CURVE_POINT));

        const FloatPoint_t &point = m_Curve->GetPoint(i);
        dc.DrawCircle(point.x * r.width,
            r.height - r.height * point.y,
            4); // TODO: make the radius a constant or a configurable param
    }
}

void c_ToneCurveEditor::DrawCurve(
    wxDC &dc,
    const wxRect &r ///< Represents the drawing area
)
{
    dc.SetPen(*wxBLACK_PEN);

    wxPoint prev(0, r.height - r.height * m_Curve->GetPreciseValue(0.0f));

    int step = 1;
    if (m_NumDrawSegments != 0)
        step = r.width/m_NumDrawSegments;

    if (step == 0)
        step = 1;

    for (int x = 1; x < r.width; x += step)
    {
        int y = r.height - r.height * m_Curve->GetPreciseValue((float)x / r.width);
        dc.DrawLine(prev.x, prev.y, x, y);
        prev.x = x;
        prev.y = y;
    }

    MarkCurvePoints(dc, r);
}

void c_ToneCurveEditor::DrawHistogram(
    wxDC &dc,
    const wxRect &r ///< Represents the drawing area
    )
{
    if (!m_Histogram.values.empty())
    {
        dc.SetPen(*wxTRANSPARENT_PEN);
        dc.SetBrush(wxBrush(CL_HISTOGRAM, wxBRUSHSTYLE_SOLID));

        int step = 1;
        if (m_NumDrawSegments != 0)
            step = std::max((size_t)1, r.width/m_NumDrawSegments);

        for (int x = 0; x < r.width; x += step)
        {
            int displayedValue;
            int histogramValue = m_Histogram.values[x * m_Histogram.values.size() / r.width];

            if (m_LogarithmicHistogram)
            {
                if (m_Histogram.maxCount == 0)
                    displayedValue = 0;
                else
                    displayedValue = (r.height-1) * (histogramValue > 0 ? logf(histogramValue) : 0) / logf(m_Histogram.maxCount);
            }
            else
                displayedValue = (r.height-1) * histogramValue / m_Histogram.maxCount;

            dc.DrawRectangle(x, (r.height-1) - displayedValue, step, displayedValue);
        }
    }
}

/// Uses double buffering; therefore, for all calls to Refresh() we specify eraseBackground = false
void c_ToneCurveEditor::OnPaintCurveArea(wxPaintEvent &event)
{
    wxPaintDC dc(m_CurveArea);

    // Draw to a bitmap first to avoid flickering
    wxBitmap bmp(m_CurveArea->GetClientRect().width,
                 m_CurveArea->GetClientRect().height);
    wxMemoryDC bmpDC(bmp);
    bmpDC.SetBrush(wxBrush(CL_CURVE_BACKGROUND));

    // Clear the contents
    bmpDC.SetPen(*wxTRANSPARENT_PEN);
    bmpDC.DrawRectangle(m_CurveArea->GetClientRect());

    DrawHistogram(bmpDC, m_CurveArea->GetClientRect());
    DrawCurve(bmpDC, m_CurveArea->GetClientRect());

    dc.Blit(wxPoint(0, 0), m_CurveArea->GetClientSize(), &bmpDC, wxPoint(0, 0));
}

void c_ToneCurveEditor::SetHistogramLogarithmic(bool value)
{
    if (value != m_LogarithmicHistogram)
        Refresh(false);

    m_LogarithmicHistogram = value;
    ((wxToggleButton *)FindWindowById(ID_Logarithmic, this))->SetValue(value);
}

/// Sends EVT_TONE_CURVE to parent, but not more often than the delay specified in the constructor
void c_ToneCurveEditor::DoPerformAction(void *param)
{
    GetParent()->GetEventHandler()->AddPendingEvent(wxCommandEvent(EVT_TONE_CURVE, GetId()));
}

c_ToneCurveEditor::c_ToneCurveEditor(wxWindow *parent, c_ToneCurve *curve, int id,
    unsigned updateEvtDelay, ///< Delay in milliseconds between sending subsequent EVT_TONE_CURVE events)
    bool logarithmicHistogram ///< If true, histogram is displayed in logarithmic scale
)
: m_Curve(curve), m_LogarithmicHistogram(logarithmicHistogram)
{
    Create(parent, id, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE | wxFULL_REPAINT_ON_RESIZE);

    m_MouseOps.draggedPointIdx = -1;
    m_NumDrawSegments = Configuration::ToneCurveEditorNumDrawSegments;

    SetActionDelay(updateEvtDelay);

    // Init sizers and child controls ---------------------------------

    wxSizer *szTop = new wxBoxSizer(wxVERTICAL);

    m_CurveArea = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxSize(100, 100));
    m_CurveArea->Bind(wxEVT_PAINT,              &c_ToneCurveEditor::OnPaintCurveArea, this);
    m_CurveArea->Bind(wxEVT_LEFT_DOWN,          &c_ToneCurveEditor::OnCurveAreaLeftDown, this);
    m_CurveArea->Bind(wxEVT_LEFT_UP,            &c_ToneCurveEditor::OnCurveAreaLeftUp, this);
    m_CurveArea->Bind(wxEVT_RIGHT_DOWN,         &c_ToneCurveEditor::OnCurveAreaRightDown, this);
    m_CurveArea->Bind(wxEVT_MOTION,             &c_ToneCurveEditor::OnCurveAreaMouseMove, this);
    m_CurveArea->Bind(wxEVT_MOUSE_CAPTURE_LOST, &c_ToneCurveEditor::OnMouseCaptureLost, this);

    szTop->Add(new wxStaticText(this, wxID_ANY, _("Left click: add/move point, right click: delete point")), 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxALL, BORDER);

    // Only 'm_CurveArea' has its "proportion" set to 1, so it can fill the parent, while the remaining controls keep their size
    szTop->Add(m_CurveArea, 1, wxALIGN_CENTER | wxALL | wxGROW, BORDER);

        wxSizer *szSettings = new wxBoxSizer(wxHORIZONTAL);
        szSettings->Add(new wxToggleButton(this, ID_Logarithmic, _("log"), wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT),
            0, wxALIGN_CENTER_VERTICAL | wxALL, BORDER);
        szSettings->Add(new wxButton(this, ID_Reset, _("reset"), wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT),
            0, wxALIGN_CENTER_VERTICAL | wxALL, BORDER);
        szSettings->Add(new wxButton(this, ID_Invert, _("inv"), wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT),
            0, wxALIGN_CENTER_VERTICAL | wxALL, BORDER);
        szSettings->Add(new wxButton(this, ID_Stretch, _("stretch"), wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT),
            0, wxALIGN_CENTER_VERTICAL | wxALL, BORDER);
        szSettings->Add(new wxCheckBox(this, ID_Smooth, _("smooth")), 0, wxALIGN_CENTER_VERTICAL | wxALL, BORDER);
        szSettings->Add(new wxCheckBox(this, ID_GammaCheckBox, _("gamma")), 0, wxALIGN_CENTER_VERTICAL | wxALL, BORDER);
        szSettings->Add(new wxSpinCtrlDouble(this, ID_GammaCtrl, wxEmptyString, wxDefaultPosition, wxDefaultSize,
                wxSP_ARROW_KEYS, 0.05, 10.0, 1.0, 0.05), 0, wxALIGN_CENTER_VERTICAL | wxALL, BORDER);

        szTop->Add(szSettings, 0, wxALIGN_LEFT | wxALL, BORDER);

    SetSizer(szTop);
    SetToolTips();

    ((wxCheckBox *)FindWindowById(ID_Smooth, this))->SetValue(m_Curve->GetSmooth());

    m_MouseOps.dragging = false;

    ((wxToggleButton *)FindWindowById(ID_Logarithmic, this))->SetValue(logarithmicHistogram);

    m_CurveArea->SetCursor(*wxCROSS_CURSOR);
    Fit();
}

void c_ToneCurveEditor::SetToolTips()
{
    FindWindowById(ID_Reset, this)->SetToolTip(_("Reset the curve to identity map: a line from (0,0) to (1,1)"));
    FindWindowById(ID_Smooth, this)->SetToolTip(_("Smooth curve"));
    FindWindowById(ID_Logarithmic, this)->SetToolTip(_("Use logarithmic scale for histogram values"));
    FindWindowById(ID_GammaCheckBox, this)->SetToolTip(_("Use gamma curve (overrules graph)"));
    FindWindowById(ID_Invert, this)->SetToolTip(_("Invert brightness levels\n(reflect curve points horizontally)"));
    FindWindowById(ID_Stretch, this)->SetToolTip(_("Stretch the curve to cover the histogram only"));
}
