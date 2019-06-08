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
    Batch progress dialog implementation.
*/

#include <limits.h>
#include <optional>
#include <string>
#include <wx/button.h>
#include <wx/dialog.h>
#include <wx/event.h>
#include <wx/filedlg.h>
#include <wx/gauge.h>
#include <wx/grid.h>
#include <wx/msgdlg.h>
#include <wx/sizer.h>
#include <wx/statline.h>

#include "appconfig.h"
#include "batch_params.h"
#include "batch.h"
#include "bmp.h"
#include "ctrl_ids.h"
#include "image.h"
#include "imppg_assert.h"
#include "logging.h"
#include "settings.h"
#include "tiff.h"
#include "w_lrdeconv.h"
#include "w_tcurve.h"
#include "w_unshmask.h"
#include "worker.h"

const size_t FILE_IDX_NONE = UINT_MAX;

class c_BatchDialog: public wxDialog
{
    void OnCommandEvent(wxCommandEvent& event);
    void OnThreadEvent(wxThreadEvent& event);
    void OnInit(wxInitDialogEvent& event);
    void OnIdle(wxIdleEvent& event);
    void OnClose(wxCloseEvent& event);

    void InitControls();

    wxGrid m_Grid;
    wxArrayString m_FileNames;

    wxGauge* m_ProgressCtrl{nullptr};

    bool m_FileOperationFailure;

    struct
    {
        bool loadedSuccessfully;

        bool hasLR;       ///< 'True' if Lucy-Richardson settings are specified
        bool hasUnshMask; ///< 'True' if unsharp masking settings are specified
        bool hasTCurve;   ///< 'True' if tone curve is specified

        float lrSigma;
        int lrIters;

        // See comments in c_UnsharpMaskingThread::DoWork() for details
        bool unshAdaptive;
        float unshSigma, unshAmountMin, unshAmountMax, unshThreshold, unshWidth;

        bool deringing;

        struct
        {
            bool enabled;
            float min, max;
        } normalization;

        c_ToneCurve tcurve;

        wxString outputDir;
        OutputFormat outputFmt;
    } m_Settings;

    size_t m_CurrentFile; ///< Index of the currently processed file
    std::optional<c_Image> m_RawImg; ///< Raw (unprocessed, but possibly normalized) current image
    std::optional<c_Image> m_Img; ///< Currently processed image (first the original, later a result of one of the processing steps)
    ProcessingRequest m_ProcessingRequest;
    int m_ThreadId; ///< Unique worker thread id (not reused by new threads)

    ExclusiveAccessObject<IWorkerThread*> m_Worker{nullptr};

    /// Starts processing of the next file
    void ProcessNextFile();
    void ScheduleProcessing(ProcessingRequest request);
    /// Updates the progress string in the files grid
    void SetProgressInfo(wxString info);
    bool IsProcessingInProgress()
    {
        auto lock = m_Worker.Lock();
        return lock.Get() != nullptr;
    }
    /// Returns 'false' on error
    bool SaveOutputFile();
    void OnProcessingStepCompleted(CompletionStatus status);

public:
    c_BatchDialog(wxWindow* parent, const wxArrayString& fileNames,
        wxString settingsFileName,
        wxString outputDirectory,
        OutputFormat outputFormat
        );

    DECLARE_EVENT_TABLE()
};

BEGIN_EVENT_TABLE(c_BatchDialog, wxDialog)
    EVT_BUTTON(ID_Close, c_BatchDialog::OnCommandEvent)
    EVT_THREAD(ID_FINISHED_PROCESSING, c_BatchDialog::OnThreadEvent)
    EVT_THREAD(ID_PROCESSING_PROGRESS, c_BatchDialog::OnThreadEvent)
    EVT_INIT_DIALOG(c_BatchDialog::OnInit)
    EVT_IDLE(c_BatchDialog::OnIdle)
    EVT_CLOSE(c_BatchDialog::OnClose)
END_EVENT_TABLE()

/// Distance (in pixels) between controls
const int BORDER = 5;

void c_BatchDialog::OnClose(wxCloseEvent& event)
{
    { auto lock = m_Worker.Lock();
        if (lock.Get())
        {
            Log::Print("Closing of the batch processing dialog requested. Waiting for the worker thread to finish...\n");
            lock.Get()->AbortProcessing();
        }
    }

    // If it was running, the worker thread will destroy itself any moment; keep polling
    while (IsProcessingInProgress())
        wxThread::Yield();
    Log::Print("Closing the batch processing dialog.\n");
    event.Skip();
}

void c_BatchDialog::OnInit(wxInitDialogEvent&)
{
    if (m_Settings.loadedSuccessfully)
        ProcessNextFile();
}

void c_BatchDialog::OnIdle(wxIdleEvent& event)
{
    if (!m_Settings.loadedSuccessfully || m_FileOperationFailure)
        Close();

    if (event.GetId() == ID_ProcessingStepSkipped)
    {
        OnProcessingStepCompleted(CompletionStatus::COMPLETED);
    }
}

/// Updates the progress string in the files grid
void c_BatchDialog::SetProgressInfo(wxString info)
{
    m_Grid.SetCellValue(m_CurrentFile, 1, info);

    int newProgressColWidth = m_Grid.GetTextExtent(info).GetWidth() + 10;
    if (m_Grid.GetColSize(1) < newProgressColWidth)
        m_Grid.SetColSize(1, newProgressColWidth);
}

/// Returns 'false' on error
bool c_BatchDialog::SaveOutputFile()
{
    wxFileName fn(m_FileNames[m_CurrentFile]);
    switch (m_Settings.outputFmt)
    {
    case OutputFormat::BMP_8: fn.SetExt("bmp"); break;
#if USE_FREEIMAGE
    case OutputFormat::PNG_8: fn.SetExt("png"); break;
#endif

    case OutputFormat::TIFF_16:
#if (USE_FREEIMAGE)
    case OutputFormat::TIFF_8_LZW:
    case OutputFormat::TIFF_16_ZIP:
    case OutputFormat::TIFF_32F:
    case OutputFormat::TIFF_32F_ZIP:
#endif
        fn.SetExt("tif");
        break;

#if USE_CFITSIO
    case OutputFormat::FITS_8:
    case OutputFormat::FITS_16:
    case OutputFormat::FITS_32F:
        fn.SetExt("fit");
        break;
#endif

    default: break;
    }

    wxString destPath = wxFileName(m_Settings.outputDir, fn.GetName() + "_out", fn.GetExt()).GetFullPath();
    if (!m_Img.value().SaveToFile(destPath.ToStdString(), m_Settings.outputFmt))
    {
        wxMessageBox(wxString::Format(_("Could not save output file: %s"), destPath), _("Error"), wxICON_ERROR, this);
        m_FileOperationFailure = true;
        return false;
    }

    return true;
}

void c_BatchDialog::OnProcessingStepCompleted(CompletionStatus status)
{
    if (status == CompletionStatus::COMPLETED)
    {
        bool fileProcessingCompleted = false;

        if (m_ProcessingRequest == ProcessingRequest::TONE_CURVE)
        {
            fileProcessingCompleted = true;
        }
        else if (m_ProcessingRequest == ProcessingRequest::UNSHARP_MASKING)
        {
            if (m_Settings.hasTCurve)
            {
                ScheduleProcessing(ProcessingRequest::TONE_CURVE);
            }
            else
                fileProcessingCompleted = true;
        }
        else if (m_ProcessingRequest == ProcessingRequest::SHARPENING)
        {
            if (m_Settings.hasUnshMask)
            {
                ScheduleProcessing(ProcessingRequest::UNSHARP_MASKING);
            }
            else if (m_Settings.hasTCurve)
            {
                ScheduleProcessing(ProcessingRequest::TONE_CURVE);
            }
            else
                fileProcessingCompleted = true;
        }

        if (fileProcessingCompleted)
        {
            if (SaveOutputFile())
                SetProgressInfo(_("Done"));
            else
                SetProgressInfo(_("Error"));

            m_Img = std::nullopt;
            m_RawImg = std::nullopt;

            if (m_CurrentFile == m_FileNames.Count() - 1) // Was it the last file?
            {
                m_ProgressCtrl->SetValue(m_FileNames.Count());
                m_CurrentFile = FILE_IDX_NONE;
                wxMessageBox(_("Processing completed."), _("Information"), wxICON_INFORMATION, this);
            }
            else
            {
                m_CurrentFile += 1;
                ProcessNextFile();
            }
        }
    }
    else
    {
        SetProgressInfo("");
        m_CurrentFile = FILE_IDX_NONE;
    }
}

void c_BatchDialog::OnThreadEvent(wxThreadEvent& event)
{
    if (event.GetInt() != m_ThreadId)
        return; // An outdated event from an older thread, ignore it

    switch (event.GetId())
    {
    case ID_PROCESSING_PROGRESS:
        {
            Log::Print(wxString::Format("Received a processing progress (%d%%) event from threadId = %d\n",
                event.GetPayload<WorkerEventPayload>().percentageComplete, event.GetInt()));

            wxString action;
            if (m_ProcessingRequest == ProcessingRequest::SHARPENING)
                action = _(L"L\u2013R deconvolution");
            else if (m_ProcessingRequest == ProcessingRequest::UNSHARP_MASKING)
                action = _("Unsharp masking");
            else if (m_ProcessingRequest == ProcessingRequest::TONE_CURVE)
                action = _("Applying tone curve");

            SetProgressInfo(wxString::Format(action + ": %d%%", event.GetPayload<WorkerEventPayload>().percentageComplete));
            break;
        }

    case ID_FINISHED_PROCESSING:
        const WorkerEventPayload& p = event.GetPayload<WorkerEventPayload>();

        Log::Print(wxString::Format("Received a processing completion event from threadId = %d, status = %s\n",
            event.GetInt(), p.completionStatus == CompletionStatus::COMPLETED ? "COMPLETED" : "ABORTED"));

        Log::Print("Waiting for the worker thread to finish... ");
        // Since we have just received the "finished processing" event, the worker thread will destroy itself any moment; keep polling
        while (IsProcessingInProgress())
            wxThread::Yield();
        Log::Print("done\n");

        m_ThreadId += 1;

        OnProcessingStepCompleted(p.completionStatus);
        break;
    }
}

void c_BatchDialog::ProcessNextFile()
{
    if (m_Settings.hasLR)
        m_ProcessingRequest = ProcessingRequest::SHARPENING;
    else if (m_Settings.hasUnshMask)
        m_ProcessingRequest = ProcessingRequest::UNSHARP_MASKING;
    else if (m_Settings.hasTCurve)
        m_ProcessingRequest = ProcessingRequest::TONE_CURVE;
    else
    {
        //TODO: move this check to before we open this dialog
        wxMessageBox(_("According to the settings file, no processing is needed."), _("Warning"), wxICON_EXCLAMATION);
        return;
    }

    if (m_CurrentFile == FILE_IDX_NONE)
    {
        m_CurrentFile = 0;
    }

    m_ProgressCtrl->SetValue(m_CurrentFile);

    ScheduleProcessing(m_ProcessingRequest);
}

void c_BatchDialog::ScheduleProcessing(ProcessingRequest request)
{
    IMPPG_ASSERT(request != ProcessingRequest::NONE);

    if (!m_Img.has_value()) // image is not yet created, i.e. `request` is the first processing step specified in the settings file
    {
        wxFileName path = wxFileName(m_FileNames[m_CurrentFile]);
        wxString ext = path.GetExt().Lower();
        std::string errorMsg;

        m_Img = LoadImageFileAsMono32f(path.GetFullPath().ToStdString(), path.GetExt().ToStdString(), &errorMsg);
        if (!m_Img)
        {
            wxMessageBox(wxString::Format(_("Could not open file: %s."), path.GetFullPath()) + (errorMsg != "" ? "\n" + errorMsg : ""),
                _("Error"), wxICON_ERROR, this);
            m_FileOperationFailure = true;
            return;
        }
        else
        {
            m_RawImg = m_Img;
        }
    }

    switch (request)
    {
    case ProcessingRequest::SHARPENING:

        if (m_Settings.normalization.enabled)
        {
            NormalizeFpImage(m_Img.value(), m_Settings.normalization.min, m_Settings.normalization.max);
            m_RawImg = m_Img;
        }

        if (m_Settings.lrIters > 0)
        {
            m_Worker = new c_LucyRichardsonThread(
                WorkerParameters{
                    *this,
                    m_Worker,
                    0,
                    m_Img.value().GetBuffer(),
                    m_Img.value().GetBuffer(),
                    m_ThreadId
                },
                m_Settings.lrSigma, m_Settings.lrIters, m_Settings.deringing, 254.0f/255, true, m_Settings.lrSigma
            );

            { auto lock = m_Worker.Lock();
                lock.Get()->Run();
            }

            SetProgressInfo(_(L"L\u2013R deconvolution") + ": 0%");
        }
        else
        {
            // Post an event instead of calling OnProcessingStepCompleted() directly,
            // because the latter would recursively enter ScheduleProcessing().
            // In the worst case (nothing to do for any of the worker threads) it would result
            // in a recursion as many levels deep as there are files to process.
            wxIdleEvent* evt = new wxIdleEvent();
            evt->SetId(ID_ProcessingStepSkipped);
            GetEventHandler()->QueueEvent(evt); // NOTE: using SendIdleEvents() would also result in a deep recursion
        }
        break;

    case ProcessingRequest::UNSHARP_MASKING:
        if (!m_Settings.unshAdaptive && m_Settings.unshAmountMax != 1.0f ||
             m_Settings.unshAdaptive && (m_Settings.unshAmountMin != 1.0f || m_Settings.unshAmountMax != 1.0f))
        {
            m_Worker = new c_UnsharpMaskingThread(
                WorkerParameters{
                    *this,
                    m_Worker,
                    0,
                    m_Img.value().GetBuffer(),
                    m_Img.value().GetBuffer(),
                    m_ThreadId
                },
                c_ImageBufferView(m_RawImg.value().GetBuffer()),
                m_Settings.unshAdaptive,
                m_Settings.unshSigma,
                m_Settings.unshAmountMin,
                m_Settings.unshAmountMax,
                m_Settings.unshThreshold,
                m_Settings.unshWidth
            );

            { auto lock = m_Worker.Lock();
                lock.Get()->Run();
            }
            SetProgressInfo(_("Unsharp masking...")); // For now unsharp masking thread doesn't send progress messages
        }
        else
        {
            wxIdleEvent* evt = new wxIdleEvent();
            evt->SetId(ID_ProcessingStepSkipped);
            GetEventHandler()->QueueEvent(evt);
        }
        break;

    case ProcessingRequest::TONE_CURVE:
        if (m_Settings.tcurve.GetNumPoints() != 2 ||
            m_Settings.tcurve.GetPoint(0).x != 0.0f ||
            m_Settings.tcurve.GetPoint(0).y != 0.0f ||
            m_Settings.tcurve.GetPoint(1).x != 1.0f ||
            m_Settings.tcurve.GetPoint(1).y != 1.0f ||
            m_Settings.tcurve.IsGammaMode() && m_Settings.tcurve.GetGamma() != 1.0f)
        {
            m_Worker = new c_ToneCurveThread(
                WorkerParameters{
                    *this,
                    m_Worker,
                    0,
                    m_Img.value().GetBuffer(),
                    m_Img.value().GetBuffer(),
                    m_ThreadId
                },
                m_Settings.tcurve,
                true
            );

            { auto lock = m_Worker.Lock();
                lock.Get()->Run();
            }
            SetProgressInfo(_("Applying tone curve") + ": 0%");
        }
        else
        {
            wxIdleEvent* evt = new wxIdleEvent();
            evt->SetId(ID_ProcessingStepSkipped);
            GetEventHandler()->QueueEvent(evt);
        }
        break;

    default: IMPPG_ABORT();
    }

    m_ProcessingRequest = request;
}

void c_BatchDialog::OnCommandEvent(wxCommandEvent& event)
{
    switch (event.GetId())
    {
    case ID_Close:
        Close();
        break;
    }
}

c_BatchDialog::c_BatchDialog(wxWindow* parent, const wxArrayString& fileNames,
    wxString settingsFileName,
    wxString outputDirectory,
    OutputFormat outputFormat
)
: wxDialog(parent, wxID_ANY, _("Batch processing"), wxDefaultPosition, wxDefaultSize,
        wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
{
    m_CurrentFile = FILE_IDX_NONE;
    m_ProcessingRequest = ProcessingRequest::NONE;
    m_ThreadId = 0;
    m_FileOperationFailure = false;

    m_FileNames = fileNames;

    if (!LoadSettings(settingsFileName,
            m_Settings.lrSigma, m_Settings.lrIters, m_Settings.deringing,
            m_Settings.unshAdaptive, m_Settings.unshSigma, m_Settings.unshAmountMin, m_Settings.unshAmountMax, m_Settings.unshThreshold, m_Settings.unshWidth,
        m_Settings.tcurve, m_Settings.normalization.enabled, m_Settings.normalization.min, m_Settings.normalization.max,
        &m_Settings.hasLR, &m_Settings.hasUnshMask, &m_Settings.hasTCurve))
    {
        wxMessageBox(_("Could not load processing settings."), _("Error"), wxICON_ERROR, parent);
        m_Settings.loadedSuccessfully = false;
    }
    else
        m_Settings.loadedSuccessfully = true;

    m_Settings.outputDir = outputDirectory;
    m_Settings.outputFmt = outputFormat;

    InitControls();
}

void c_BatchDialog::InitControls()
{
    wxBoxSizer* szTop = new wxBoxSizer(wxVERTICAL);

    szTop->Add(m_ProgressCtrl = new wxGauge(this, ID_ProgressGauge, m_FileNames.Count(), wxDefaultPosition, wxDefaultSize, wxGA_HORIZONTAL),
        0, wxALIGN_CENTER | wxALL | wxGROW, BORDER);

    m_Grid.Create(this, ID_Grid);
    m_Grid.CreateGrid(m_FileNames.Count(), 2, wxGrid::wxGridSelectRows);
    m_Grid.EnableEditing(false);
    m_Grid.DisableDragRowSize();
    m_Grid.HideRowLabels();
    m_Grid.SetColLabelValue(0, _("File"));
    m_Grid.SetColLabelValue(1, _("Progress"));
    for (size_t i = 0; i < m_FileNames.Count(); i++)
	{
        m_Grid.SetCellValue(i, 0, m_FileNames[i]);
		m_Grid.SetCellValue(i, 1, _("Waiting"));
	}
    m_Grid.AutoSizeColumns();
    szTop->Add(&m_Grid, 1, wxALIGN_CENTER | wxALL | wxGROW, BORDER);

    szTop->Add(new wxStaticLine(this), 0, wxGROW | wxALL, BORDER);

    szTop->Add(new wxButton(this, ID_Close, _("Close")), 0, wxALIGN_RIGHT | wxALL, BORDER);

    SetSizer(szTop);
    Fit();
}

/// Allows the user to choose images to process, choose the processing settings to use and displays the batch processing progress and control dialog
void BatchProcessing(
    wxWindow* parent ///< Pointer to the application's main window
)
{
    c_BatchParamsDialog batchParamsDlg(parent);
    if (batchParamsDlg.ShowModal() == wxID_OK)
    {
        c_BatchDialog batchDlg(parent,
            batchParamsDlg.GetInputFileNames(),
            batchParamsDlg.GetSettingsFileName(),
            batchParamsDlg.GetOutputDirectory(),
            batchParamsDlg.GetOutputFormat());
        wxRect r = Configuration::BatchProgressDialogPosSize;
        batchDlg.SetPosition(r.GetPosition());
        batchDlg.SetSize(r.GetSize());
        FixWindowPosition(batchDlg);
        batchDlg.ShowModal();
        Configuration::BatchProgressDialogPosSize = wxRect(batchDlg.GetPosition(), batchDlg.GetSize());
    }
    return;
}
