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
    Image alignment progress dialog.
*/

#include <wx/arrstr.h>
#include <wx/dialog.h>
#include <wx/window.h>
#include <wx/event.h>
#include <wx/sizer.h>
#include <wx/msgdlg.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/gauge.h>
#include <wx/settings.h>
#include "align.h"
#include "align_proc.h"
#include "appconfig.h"

const int BORDER = 5; ///< Border size (in pixels) between controls

class c_ImageAlignmentProgress: public wxDialog
{
    void OnInit(wxInitDialogEvent &event);
    void OnThreadEvent(wxThreadEvent &event);

    virtual void EndModal(int retCode);


    c_ImageAlignmentWorkerThread *m_WorkerThread;
    wxCriticalSection m_Guard; ///< Guards access to 'm_WorkerThread'
    wxGauge m_ProgressGauge;
    wxStaticText m_InfoText;
    wxTextCtrl m_InfoLog;

    bool IsProcessingInProgress();
    void InitControls();

    AlignmentParameters_t m_Parameters;

public:
    c_ImageAlignmentProgress(wxWindow *parent, wxWindowID id, AlignmentParameters_t &params);


    DECLARE_EVENT_TABLE()
};

//--------------------------------------------------------------------------------

BEGIN_EVENT_TABLE(c_ImageAlignmentProgress, wxDialog)
    EVT_THREAD(EID_ABORTED,                     c_ImageAlignmentProgress::OnThreadEvent)
    EVT_THREAD(EID_COMPLETED,                   c_ImageAlignmentProgress::OnThreadEvent)
    EVT_THREAD(EID_PHASECORR_IMG_TRANSLATION,   c_ImageAlignmentProgress::OnThreadEvent)
    EVT_THREAD(EID_SAVED_OUTPUT_IMAGE,          c_ImageAlignmentProgress::OnThreadEvent)
    EVT_THREAD(EID_LIMB_FOUND_DISC_RADIUS,      c_ImageAlignmentProgress::OnThreadEvent)
    EVT_THREAD(EID_LIMB_USING_RADIUS,           c_ImageAlignmentProgress::OnThreadEvent)
    EVT_THREAD(EID_LIMB_STABILIZATION_PROGRESS, c_ImageAlignmentProgress::OnThreadEvent)
    EVT_THREAD(EID_LIMB_STABILIZATION_FAILURE,  c_ImageAlignmentProgress::OnThreadEvent)
    EVT_INIT_DIALOG(c_ImageAlignmentProgress::OnInit)
END_EVENT_TABLE()

void c_ImageAlignmentProgress::InitControls()
{
    wxSizer *szTop = new wxBoxSizer(wxVERTICAL);

    m_InfoText.Create(this, wxID_ANY, wxEmptyString);
    m_InfoText.SetFont(m_InfoText.GetFont().MakeBold());
    if (m_Parameters.alignmentMethod == ALM_PHASE_CORRELATION)
        m_InfoText.SetLabel(_("Determining translation vectors..."));
    szTop->Add(&m_InfoText, 0, wxALIGN_LEFT | wxGROW | wxALL, BORDER);

    //TODO: use wxGA_PROGRESS style if wxWidgets >=3.1
    m_ProgressGauge.Create(this, wxID_ANY, 100);
    // Initially set the range to 1 less than image count, because first we show
    // the completed translations (starting with the second image).
    m_ProgressGauge.SetRange(m_Parameters.inputFiles.Count() - 1);
    szTop->Add(&m_ProgressGauge, 0, wxALIGN_CENTER | wxGROW | wxALL, BORDER);

    m_InfoLog.Create(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE | wxTE_READONLY | wxTE_DONTWRAP);
    m_InfoLog.SetBackgroundColour(this->GetBackgroundColour());
    szTop->Add(&m_InfoLog, 1, wxALIGN_CENTER | wxGROW | wxALL, BORDER);

    szTop->Add(CreateSeparatedButtonSizer(wxCANCEL), 0, wxGROW | wxALL, BORDER);

    SetSizer(szTop);
    Fit();
}

void c_ImageAlignmentProgress::OnThreadEvent(wxThreadEvent &event)
{
    AlignmentEventPayload_t payload;

    switch (event.GetId())
    {
    case EID_ABORTED:
    case EID_COMPLETED:
        while (IsProcessingInProgress())
            wxThread::Yield();
        if (event.GetId() == EID_ABORTED)
        {
            wxMessageBox(event.GetString(), _("Aborted"), wxICON_ERROR, this);
            EndModal(wxID_CANCEL);
        }
        else
        {
            wxMessageBox(_("Processing completed."), _("Information"), wxICON_INFORMATION, this);
        }
        break;

    case EID_PHASECORR_IMG_TRANSLATION:
        m_ProgressGauge.SetValue(event.GetInt());

        payload = event.GetPayload<AlignmentEventPayload_t>();
        m_InfoLog.AppendText(wxString::Format(_("Image %d/%d: translated by %.2f, %.2f."),
            event.GetInt()+1,
            (int)m_Parameters.inputFiles.Count(),
            payload.translation.x, payload.translation.y) + "\n");

        break;

    case EID_SAVED_OUTPUT_IMAGE:
        if (event.GetInt() == 0)
        {
            m_InfoText.SetLabel(_("Translating and saving output images..."));

            m_ProgressGauge.SetRange(m_Parameters.inputFiles.Count());
            m_InfoLog.AppendText("\n");
        }

        m_ProgressGauge.SetValue(event.GetInt() + 1);

        m_InfoLog.AppendText(wxString::Format(_("Translated and saved image %d/%d."),
            event.GetInt()+1, (int)m_Parameters.inputFiles.Count()) + "\n");

        break;

    case EID_LIMB_USING_RADIUS:
        payload = event.GetPayload<AlignmentEventPayload_t>();
        m_InfoLog.AppendText(wxString::Format(_("Using average radius %.2f."), payload.radius) + "\n");
        break;

    case EID_LIMB_STABILIZATION_PROGRESS:
        if (event.GetInt() == 0)
        {
            m_InfoText.SetLabel(_("Performing final stabilization..."));

            m_ProgressGauge.SetRange(m_Parameters.inputFiles.Count());
        }

        m_ProgressGauge.SetValue(event.GetInt() + 1);
        break;

    case EID_LIMB_STABILIZATION_FAILURE:
        m_InfoLog.AppendText(event.GetString() + "\n");
        break;

    case EID_LIMB_FOUND_DISC_RADIUS:
        payload = event.GetPayload<AlignmentEventPayload_t>();
        if (event.GetInt() == 0)
        {
            m_InfoText.SetLabel(_("Determining disc radius in images..."));

            m_ProgressGauge.SetRange(m_Parameters.inputFiles.Count());
            m_InfoLog.AppendText("\n");
        }

        m_InfoLog.AppendText(wxString::Format(_("Image %d/%d: disc radius = %.2f"),
            event.GetInt()+1,
            (int)m_Parameters.inputFiles.Count(),
            payload.radius) + "\n");
        m_ProgressGauge.SetValue(event.GetInt() + 1);

        break;
    }
}

bool c_ImageAlignmentProgress::IsProcessingInProgress()
{
    { wxCriticalSectionLocker lock(m_Guard);
        return (0 != m_WorkerThread);
    }
}

c_ImageAlignmentProgress::c_ImageAlignmentProgress(wxWindow *parent, wxWindowID id, AlignmentParameters_t &params)
: wxDialog(parent, id, _("Image alignment progress"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
m_Parameters(params)
{
    m_WorkerThread = 0;

    InitControls();
}

void c_ImageAlignmentProgress::OnInit(wxInitDialogEvent &event)
{
    m_WorkerThread = new c_ImageAlignmentWorkerThread(*this, m_Guard, &m_WorkerThread, m_Parameters);
    m_WorkerThread->Run();
}

void c_ImageAlignmentProgress::EndModal(int retCode)
{
    if (IsProcessingInProgress())
    {
        { wxCriticalSectionLocker lock(m_Guard);
            if (m_WorkerThread)
                m_WorkerThread->AbortProcessing();
        }

        // If it was running, the worker thread will destroy itself any moment; keep polling
        while (IsProcessingInProgress())
            wxThread::Yield();
    }

    wxDialog::EndModal(retCode);
}

/// Displays the alignment progress dialog and starts processing. Returns 'true' if processing has completed.
bool AlignImages(wxWindow *parent, AlignmentParameters_t &params)
{
    c_ImageAlignmentProgress dlg(parent, wxID_ANY, params);
    wxRect r = Configuration::AlignProgressDialogPosSize;
    dlg.SetPosition(r.GetPosition());
    dlg.SetSize(r.GetSize());
    FixWindowPosition(dlg);
    int result = dlg.ShowModal();
    Configuration::AlignProgressDialogPosSize = wxRect(dlg.GetPosition(), dlg.GetSize());
    return wxID_OK == result;
}
