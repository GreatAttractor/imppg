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
    Numerical value editor header.
*/

#ifndef IMPPG_NUMERICAL_CTRL_H
#define IMPPG_NUMERICAL_CTRL_H

#include <wx/panel.h>
#include <wx/spinctrl.h>
#include <wx/slider.h>
#include <wx/stattext.h>
#include <wx/event.h>
#include "daction.h"

// Event sent to parent every time the control's value changes
wxDECLARE_EVENT(EVT_NUMERICAL_CTRL, wxCommandEvent);

/// Convenient editor of a numerical value
/** Contains a spin control and a slider for fine-tuning the entered value.
    The slider has always the range of:
        [current_val/rangeFactor; currentVal*rangeFactor]

    where rangeFactor is > 1. The slider returns to the middle position after every use. */
class c_NumericalCtrl: public wxPanel, IDelayedAction
{
    double m_Value;
    double m_SliderRangeFactor; ///< TODO: write an explanation
    bool m_AutoAdjustNumSteps;

    wxStaticText m_Label;
    wxSlider* m_SliderCtrl{nullptr};
    wxSpinCtrlDouble* m_SpinCtrl{nullptr};

    struct
    {
        bool grabbed;
        double grabbedVal;
    } Scrolling;

    void OnValueChanged();
    void OnSpinEvent(wxSpinDoubleEvent& event);
#ifdef __WXMSW__
    void OnSpinEnter(wxCommandEvent& event);
#endif
    void OnSliderScroll(wxScrollEvent& event);
    void OnSliderEndScroll(wxScrollEvent& event);
    void OnSliderSize(wxSizeEvent& event);
    void OnSliderKeyDown(wxKeyEvent& event);
    void OnSliderKeyUp(wxKeyEvent& event);
    void DoPerformAction(void* param = nullptr) override;

public:
    c_NumericalCtrl(wxWindow* parent, int id,
        wxString label, ///< Label displayed next to the spin control
        double minVal, ///< Min. allowed value
        double maxVal, ///< Max. allowed value
        double initialVal, ///< Initial value
        double spinCtrlStep, ///< Inc./dec. step of the spin control
        int numDigits, ///< Number of digits to display
        /// For spin control's value 'v' the range of the slider is [v/sliderRangeFactor, v*sliderRangeFactor]
        double sliderRangeFactor,
        int numSliderSteps, ///< Number of slider steps
        bool autoAdjustNumSteps = false, ///< If true, the slider will always have 1 step per pixel (adjusted after every resize)
        unsigned updateEvtDelay = 0 ///< Delay in milliseconds between sending subsequent EVT_NUMERICAL_CTRL events
        );

    double GetValue() const
    { return m_Value; }

    /// Sets the value; does not generate an EVT_NUMERICAL_CTRL event
    void SetValue(double value);

    void SetLabel(const wxString& label) override;

    DECLARE_EVENT_TABLE()
};

#endif
