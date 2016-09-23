/*
ImPPG (Image Post-Processor) - common operations for astronomical stacks and other images
Copyright (C) 2015, 2016 Filip Szczerek <ga.software@yahoo.com>

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
    Numerical value editor implementation.
*/

#include <cmath>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/valnum.h>
#include "num_ctrl.h"

/// Border size in pixels between constituent controls
const int BORDER = 5;

enum
{
    ID_SpinCtrl = wxID_HIGHEST + 1,
    ID_Slider
};

// Event sent to parent every time the control's value changes
wxDEFINE_EVENT(EVT_NUMERICAL_CTRL, wxCommandEvent);

BEGIN_EVENT_TABLE(c_NumericalCtrl, wxWindow)
    EVT_SPINCTRLDOUBLE(ID_SpinCtrl, c_NumericalCtrl::OnSpinEvent)
#ifdef __WXMSW__
	// On MS Windows, wxWidgets 3.0.2, the control doesn't generate EVT_SPINCTRLDOUBLE on Enter;
	// needs a separate handler.
    EVT_TEXT_ENTER(ID_SpinCtrl, c_NumericalCtrl::OnSpinEnter)
#endif
    EVT_SCROLL_THUMBTRACK(c_NumericalCtrl::OnSliderScroll)
    EVT_SCROLL_THUMBRELEASE(c_NumericalCtrl::OnSliderEndScroll)

    EVT_SCROLL_BOTTOM(c_NumericalCtrl::OnSliderEndScroll)
    EVT_SCROLL_TOP(c_NumericalCtrl::OnSliderEndScroll)
    EVT_SCROLL_PAGEDOWN(c_NumericalCtrl::OnSliderEndScroll)
    EVT_SCROLL_PAGEUP(c_NumericalCtrl::OnSliderEndScroll)
END_EVENT_TABLE()

c_NumericalCtrl::c_NumericalCtrl(wxWindow *parent, int id,
    wxString label, ///< Label displayed next to the spin control
    double minVal, ///< Min. allowed value
    double maxVal, ///< Max. allowed value
    double initialVal, ///< Initial value
    double spinCtrlStep, ///< Inc./dec. step of the spin control
    int numDigits, ///< Number of digits to display
    /// For spin control's value 'v' the range of the slider is [v/sliderRangeFactor, v*sliderRangeFactor]
    double sliderRangeFactor,
    int numSliderSteps, ///< Number of slider steps
    bool autoAdjustNumSteps, ///< If true, the slider will always have 1 step per pixel (adjusted after every resize)
    unsigned updateEvtDelay ///< Delay in milliseconds between sending subsequent EVT_NUMERICAL_CTRL events
)
: wxPanel(parent, id), m_Value(initialVal), m_SliderRangeFactor(sliderRangeFactor), m_AutoAdjustNumSteps(autoAdjustNumSteps)
{
    SetActionDelay(updateEvtDelay);
    Scrolling.grabbed = false;

    wxSizer *szTop = new wxBoxSizer(wxVERTICAL);

    wxSizer *szSpinCtrl = new wxBoxSizer(wxHORIZONTAL);
    m_Label.Create(this, wxID_ANY, label);
    szSpinCtrl->Add(&m_Label, 0, wxALIGN_CENTER_VERTICAL | wxALL, BORDER);
        wxSpinCtrlDouble *spinCtrl = new wxSpinCtrlDouble(this, ID_SpinCtrl, wxString::Format("%f", initialVal),
            wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS | wxTE_PROCESS_ENTER, minVal, maxVal, initialVal, spinCtrlStep);
        spinCtrl->SetDigits(numDigits);
        szSpinCtrl->Add(spinCtrl, 0, wxALIGN_CENTER_VERTICAL | wxGROW | wxALL, BORDER);
        szTop->Add(szSpinCtrl, 0, wxALIGN_LEFT | wxALL, BORDER);

    if (autoAdjustNumSteps && numSliderSteps <= 0)
        numSliderSteps = 2;

    wxSlider *slider = new wxSlider(this, ID_Slider, numSliderSteps/2, 0, numSliderSteps-1);
    slider->Bind(wxEVT_SIZE, &c_NumericalCtrl::OnSliderSize, this);
    slider->Bind(wxEVT_KEY_DOWN, &c_NumericalCtrl::OnSliderKeyDown, this);
    slider->Bind(wxEVT_KEY_UP, &c_NumericalCtrl::OnSliderKeyUp, this);

    szTop->Add(slider, 0, wxALIGN_CENTER_HORIZONTAL | wxGROW | wxALL, BORDER);

    SetSizer(szTop);
}

void c_NumericalCtrl::OnSliderSize(wxSizeEvent &event)
{
    if (m_AutoAdjustNumSteps)
    {
        wxSlider *slider = (wxSlider *)FindWindowById(ID_Slider, this);
        slider->SetMax(slider->GetSize().GetWidth());
        slider->SetValue(slider->GetMax()/2);
    }
    event.Skip();
}

void c_NumericalCtrl::OnSliderEndScroll(wxScrollEvent &event)
{
    if (event.GetId() == ID_Slider)
    {
        wxSlider *slider = (wxSlider *)FindWindowById(ID_Slider, this);

        Scrolling.grabbed = false;
        slider->SetValue((slider->GetMax() + 1)/2);
    }
}


void c_NumericalCtrl::OnSliderScroll(wxScrollEvent &event)
{
    if (event.GetId() == ID_Slider)
    {
        wxSpinCtrlDouble *spinCtrl = (wxSpinCtrlDouble *)FindWindowById(ID_SpinCtrl, this);
        wxSlider *slider = (wxSlider *)FindWindowById(ID_Slider, this);

        if (!Scrolling.grabbed)
        {
            Scrolling.grabbed = true;
            Scrolling.grabbedVal = spinCtrl->GetValue();
        }

        if (Scrolling.grabbedVal == 0.0) // Possible if spin ctrl's minVal is 0
        {
            // Bump the value up a bit, otherwise the slider alone wouldn't increase it (due to the formula below)
            Scrolling.grabbedVal = std::pow(10.0, -(int)spinCtrl->GetDigits());
        }

        double newVal = Scrolling.grabbedVal * std::pow(m_SliderRangeFactor, 2.0 * slider->GetValue()/slider->GetMax() - 1);

        spinCtrl->SetValue(newVal);
        OnValueChanged();
    }
}

#ifdef __WXMSW__
void c_NumericalCtrl::OnSpinEnter(wxCommandEvent &event)
{
    // In wxWidgets 3.0.2 wxSpinCtrlDouble does not update its value
    // after Enter is pressed. As a workaround, we simulate the "focus lost" event.
    wxFocusEvent evt;
    evt.SetEventType(wxEVT_KILL_FOCUS);
    ((wxSpinCtrlDouble *)FindWindowById(ID_SpinCtrl, this))->GetEventHandler()->ProcessEvent(evt); // This will cause OnValueChanged() to execute
}
#endif

void c_NumericalCtrl::OnValueChanged()
{
    m_Value = ((wxSpinCtrlDouble *)FindWindowById(ID_SpinCtrl, this))->GetValue();
    DelayedAction();
}

/// Sets the value; does not generate an EVT_NUMERICAL_CTRL event
void c_NumericalCtrl::SetValue(double value)
{
    m_Value = value;
    ((wxSpinCtrlDouble *)FindWindowById(ID_SpinCtrl, this))->SetValue(value);
}

void c_NumericalCtrl::OnSpinEvent(wxSpinDoubleEvent &event)
{
    OnValueChanged();
}

void c_NumericalCtrl::DoPerformAction(void *param)
{
    GetParent()->GetEventHandler()->AddPendingEvent(wxCommandEvent(EVT_NUMERICAL_CTRL, GetId()));
}

void c_NumericalCtrl::OnSliderKeyDown(wxKeyEvent &event)
{
    if (event.GetKeyCode() == WXK_LEFT || event.GetKeyCode() == WXK_RIGHT)
    {
        wxScrollEvent screvt;
        screvt.SetId(ID_Slider);
        OnSliderScroll(screvt);
        event.Skip();
    }
}

void c_NumericalCtrl::OnSliderKeyUp(wxKeyEvent &event)
{
    if (event.GetKeyCode() == WXK_LEFT || event.GetKeyCode() == WXK_RIGHT)
    {
        wxScrollEvent screvt;
        screvt.SetId(ID_Slider);
        OnSliderEndScroll(screvt);
        event.Skip();
    }
}

void c_NumericalCtrl::SetLabel(const wxString &label)
{
    m_Label.SetLabel(label);
}
