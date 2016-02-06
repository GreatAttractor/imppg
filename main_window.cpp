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
    Main window class implementation.
*/

#include <cassert>
#include <string>
#include <float.h>
#include <algorithm>
#include <cmath>
#include <boost/scoped_ptr.hpp>
#include <wx/intl.h>
#include <wx/aui/framemanager.h>
#include <wx/textctrl.h>
#include <wx/dcclient.h>
#include <wx/toolbar.h>
#include <wx/menu.h>
#include <wx/menuitem.h>
#include <wx/image.h>
#include <wx/dcmemory.h>
#include <wx/scrolwin.h>
#include <wx/msgdlg.h>
#include <wx/filedlg.h>
#include <wx/filename.h>
#include <wx/sizer.h>
#include <wx/slider.h>
#include <wx/stattext.h>
#include <wx/valnum.h>
#include <wx/button.h>
#include <wx/pen.h>
#include <wx/spinctrl.h>
#include <wx/tglbtn.h>
#include <wx/notebook.h>
#include <wx/bmpbuttn.h>
#include <wx/region.h>
#include <wx/checkbox.h>
#include <wx/choicdlg.h>
#include <wx/numdlg.h>
#include <wx/valgen.h>
#include <wx/stdpaths.h>
#include "main_window.h"
#include "bmp.h"
#include "tiff.h"
#include "tcrv_edit.h"
#include "w_lrdeconv.h"
#include "w_tcurve.h"
#include "w_unshmask.h"
#include "num_ctrl.h"
#include "logging.h"
#include "appconfig.h"
#include "settings.h"
#include "batch.h"
#include "normalize.h"
#include "lrdeconv.h"
#include "ctrl_ids.h"
#include "formats.h"
#include "align.h"
#include "wxapp.h"

DECLARE_APP(c_MyApp);

const int BORDER = 5; ///< Border size (in pixels) around controls in sizers
const int REAL_PREC = 4; ///< Precision of real numbers in text controls

const int NUM_HISTOGRAM_BINS = 1024;

/// Number of milliseconds to wait after a scroll or resize event before refreshing the display if zoom level <> 100%
const int IMAGE_SCALING_DELAY = 150;

const float ZOOM_NONE = 1.0f;
const float ZOOM_STEP = 1.5f; ///< Zoom in/zoom out factor
const float ZOOM_MIN = 0.05f;
const float ZOOM_MAX = 20.0f;

namespace Default
{
    const float LR_SIGMA = 1.3f;
    const int LR_ITERATIONS = 50;

    const float UNSHMASK_SIGMA = 1.3f;
    const float UNSHMASK_AMOUNT = 1.0f;
    const float UNSHMASK_THRESHOLD = 0.01f;
    const float UNSHMASK_WIDTH = 0.01f;
}

namespace PaneNames
{
const wxString imageView = "imageView";
const wxString processing = "processing";
}

wxImageResizeQuality GetResizeQuality(ScalingMethod_t smethod)
{
    switch (smethod)
    {
    case S_NEAREST: return wxIMAGE_QUALITY_NEAREST;
    case S_LINEAR: return wxIMAGE_QUALITY_BILINEAR;
    case S_CUBIC: return wxIMAGE_QUALITY_BICUBIC;
    default: return wxIMAGE_QUALITY_BICUBIC;
    }
}

BEGIN_EVENT_TABLE(c_MainWindow, wxFrame)
    EVT_CLOSE(c_MainWindow::OnClose)
    EVT_MENU(wxID_OPEN, c_MainWindow::OnOpenFile)
    EVT_TOOL(wxID_OPEN, c_MainWindow::OnOpenFile)
    EVT_MENU(wxID_SAVE, c_MainWindow::OnCommandEvent)
    EVT_TOOL(wxID_SAVE, c_MainWindow::OnCommandEvent)
    EVT_MENU(wxID_EXIT, c_MainWindow::OnCommandEvent)
    EVT_MENU(ID_ToggleToneCurveEditor, c_MainWindow::OnCommandEvent)
    EVT_TOOL(ID_ToggleToneCurveEditor, c_MainWindow::OnCommandEvent)
    EVT_MENU(ID_ToggleProcessingPanel, c_MainWindow::OnCommandEvent)
    EVT_TOOL(ID_ToggleProcessingPanel, c_MainWindow::OnCommandEvent)
    EVT_THREAD(ID_FINISHED_PROCESSING, c_MainWindow::OnThreadEvent)
    EVT_THREAD(ID_PROCESSING_PROGRESS, c_MainWindow::OnThreadEvent)
    EVT_COMMAND(ID_ToneCurveEditor, EVT_TONE_CURVE, c_MainWindow::OnToneCurveChanged)
    EVT_SPINCTRL(ID_LucyRichardsonIters, c_MainWindow::OnLucyRichardsonIters)
#ifdef __WXMSW__
    // On Windows an explicit Enter key handler is needed. Not using it on wxGTK,
    // because it sometimes causes a GTK crash (invalid GTK control cast).
    EVT_TEXT_ENTER(ID_LucyRichardsonIters, c_MainWindow::OnCommandEvent)
#endif
    EVT_BUTTON(ID_LucyRichardsonReset, c_MainWindow::OnCommandEvent)
    EVT_BUTTON(ID_LucyRichardsonOff, c_MainWindow::OnCommandEvent)
    EVT_COMMAND(ID_LucyRichardsonSigma, EVT_NUMERICAL_CTRL, c_MainWindow::OnCommandEvent)
    EVT_COMMAND(ID_UnsharpMaskingSigma, EVT_NUMERICAL_CTRL, c_MainWindow::OnCommandEvent)
    EVT_COMMAND(ID_UnsharpMaskingAmountMin, EVT_NUMERICAL_CTRL, c_MainWindow::OnCommandEvent)
    EVT_COMMAND(ID_UnsharpMaskingAmountMax, EVT_NUMERICAL_CTRL, c_MainWindow::OnCommandEvent)
    EVT_COMMAND(ID_UnsharpMaskingThreshold, EVT_NUMERICAL_CTRL, c_MainWindow::OnCommandEvent)
    EVT_COMMAND(ID_UnsharpMaskingWidth, EVT_NUMERICAL_CTRL, c_MainWindow::OnCommandEvent)
    EVT_BUTTON(ID_UnsharpMaskingReset, c_MainWindow::OnCommandEvent)
    EVT_CHECKBOX(ID_UnsharpMaskingAdaptive, c_MainWindow::OnCommandEvent)
    EVT_TOOL(ID_SelectAndProcessAll, c_MainWindow::OnCommandEvent)
    EVT_TOOL(ID_FitInWindow, c_MainWindow::OnCommandEvent)
    EVT_MENU(ID_FitInWindow, c_MainWindow::OnCommandEvent)
    EVT_AUI_PANE_CLOSE(c_MainWindow::OnAuiPaneClose)
    EVT_TIMER(ID_ScalingTimer, c_MainWindow::OnTimer)
    EVT_TOOL(ID_LoadSettings, c_MainWindow::OnCommandEvent)
    EVT_TOOL(ID_SaveSettings, c_MainWindow::OnCommandEvent)
    EVT_MENU(ID_BatchProcessing, c_MainWindow::OnCommandEvent)
    EVT_CHECKBOX(ID_LucyRichardsonDeringing, c_MainWindow::OnCommandEvent)
    EVT_MENU(ID_NormalizeImage, c_MainWindow::OnCommandEvent)
    EVT_MENU(ID_ChooseLanguage, c_MainWindow::OnCommandEvent)
    EVT_MENU(ID_About, c_MainWindow::OnCommandEvent)
    EVT_MENU(ID_AlignImages, c_MainWindow::OnCommandEvent)
    // The handler is bound to m_ImageView, but attach it also here to c_MainWindow
    // so that it works even if m_ImageView does not have focus
    EVT_MOUSEWHEEL(c_MainWindow::OnImageViewMouseWheel)
    EVT_MENU_RANGE(ID_ZoomRangeBegin, ID_ZoomRangeEnd, c_MainWindow::OnCommandEvent)
END_EVENT_TABLE()

void ShowAboutDialog(wxWindow *parent);

void c_MainWindow::OnImageViewScroll(wxScrollWinEvent &event)
{
    ScheduleScalingRequest();
}

/// Displays the UI language selection dialog
void c_MainWindow::SelectLanguage()
{
    static const wxString languageNames[] =
    {
        "English",
        "polski"

        // After creating a new translation file, add the language name here
    };
    // Order of items has to reflect 'languageNames'
    static const wxLanguage langIds[] =
    {
        wxLANGUAGE_ENGLISH,
        wxLANGUAGE_POLISH
    };
    const int NUM_LANGS_SUPPORTED = 2; // Has to be equal to number of elements in 'languageNames' and 'langIds'

    wxSingleChoiceDialog dlg(this, _("Choose the user interface language:"), _("Language"), NUM_LANGS_SUPPORTED, languageNames);
    for (int i = 0; i < NUM_LANGS_SUPPORTED; i++)
        if (wxGetApp().GetLanguage() == langIds[i])
        {
            dlg.SetSelection(i);
            break;
        }

    if (dlg.ShowModal() == wxID_OK)
    {
        const wxLanguageInfo *info = wxLocale::GetLanguageInfo(langIds[dlg.GetSelection()]);
        if (info)
        {
            Configuration::SetUiLanguage(info->CanonicalName);
            wxMessageBox(_("You have to restart ImPPG for the changes to take effect."), _("Information"), wxOK | wxICON_INFORMATION, this);
        }
    }
}

void c_MainWindow::OnSaveFile()
{
    if (!m_CurrentSettings || !m_CurrentSettings->m_Img)
        return;

    m_Processing.usePreciseTCurveVals = false;

    bool forcedProcessingAbort = false;
    if (IsProcessingInProgress())
    {
        if (wxYES == wxMessageBox(_("Processing in progress, abort it?"), _("Warning"), wxICON_EXCLAMATION | wxYES_NO, this))
        {
            // Signal (in a non-blocking way) the worker thread to finish ASAP.
            { wxCriticalSectionLocker lock(m_Processing.guard);
                if (m_Processing.worker)
                {
                    Log::Print("Sending abort request to the worker thread\n");
                    m_Processing.worker->AbortProcessing();
                    forcedProcessingAbort = true;
                }
            }

            while (IsProcessingInProgress())
                wxThread::Yield();
        }
        else
            return;
    }

    ProcessingSettings_t &s = *m_CurrentSettings;

    if (s.selection.x != 0 ||
        s.selection.y != 0 ||
        s.selection.width != s.m_Img->GetWidth() ||
        s.selection.height != s.m_Img->GetHeight())
    {
        if (wxYES == wxMessageBox(_("You have not selected and processed the whole image, do it now?"), _("Information"), wxICON_QUESTION | wxYES_NO, this))
        {
            // Current selection is smaller than the image. Need to select and process all.
            s.selection.SetPosition(wxPoint(0, 0));
            s.selection.SetSize(wxSize(s.m_Img->GetWidth(), s.m_Img->GetHeight()));
            s.m_FileSaveScheduled = true; // thanks to this flag, OnSaveFile() will get called once the processing scheduled below completes
            m_Processing.usePreciseTCurveVals = true;
            ScheduleProcessing(ProcessingRequest::SHARPENING);
            return;
        }
    }

    if (!forcedProcessingAbort &&
        (!s.output.toneCurve.preciseValuesApplied || !s.output.toneCurve.valid))
    {
        // If precise tone curve has not been applied yet, do it
        for (unsigned y = 0; y < s.output.toneCurve.img->GetHeight(); y++)
            for (unsigned x = 0; x < s.output.toneCurve.img->GetWidth(); x++)
                ((float *)s.output.toneCurve.img->GetRow(y))[x] = s.toneCurve.GetPreciseValue(((float *)s.output.unsharpMasking.img->GetRow(y))[x]);

        s.output.toneCurve.valid = true;
        s.output.toneCurve.preciseValuesApplied = true;
    }

    wxFileDialog dlg(this, _("Save image"), Configuration::GetFileSavePath(), wxEmptyString, GetOutputFilters(), wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (wxID_OK == dlg.ShowModal())
    {
        Configuration::SetFileSavePath(wxFileName(dlg.GetPath()).GetPath());
        if (!SaveImageFile(dlg.GetPath().ToStdString(), *s.output.toneCurve.img, (OutputFormat_t)dlg.GetFilterIndex()))
            wxMessageBox(wxString::Format(_("Could not save output file %s."), dlg.GetFilename()), _("Error"), wxICON_ERROR, this);
    }
}

void c_MainWindow::ChangeZoom(
        float newZoomFactor,
        wxPoint zoomingCenter ///< Point (physical coordinates) in m_ImageView to be kept stationary
)
{
    ProcessingSettings_t &s = *m_CurrentSettings;
    if (s.m_ImgBmp)
    {
        m_FitImageInWindow = false;
        GetToolBar()->FindById(ID_FitInWindow)->Toggle(false);
        GetToolBar()->Realize();
        GetMenuBar()->FindItem(ID_FitInWindow)->Check(false);

        float prevZoom = s.view.zoomFactor;

        s.view.zoomFactor = newZoomFactor;

        wxPoint imgViewEvtPos;

        wxPoint p = m_ImageView->CalcUnscrolledPosition(wxPoint(0, 0)) + zoomingCenter;
        p.x *= s.view.zoomFactor / prevZoom;
        p.y *= s.view.zoomFactor / prevZoom;

        // We must freeze it, because SetVirtualSize() and Scroll() (called from OnZoomChanged())
        // force an Update(), i.e. immediately refresh it on screen. We want to do this later
        // in our paint handler.
        m_ImageView->Freeze();
        OnZoomChanged(p - zoomingCenter);
        CreateScaledPreview(true);
        m_ImageView->Thaw();
    }
}

/// Must be called to finalize a zoom change
void c_MainWindow::OnZoomChanged(
    wxPoint zoomingCenter ///< Point (physical coordinates) in m_ImageView to be kept stationary
)
{
    ProcessingSettings_t &s = *m_CurrentSettings;

    if (m_FitImageInWindow)
    {
        m_ImageView->SetVirtualSize(m_ImageView->GetSize()); // disable scrolling
    }
    else
    {
        m_ImageView->SetVirtualSize(
            wxSize(s.m_ImgBmp->GetWidth()*s.view.zoomFactor,
                   s.m_ImgBmp->GetHeight()*s.view.zoomFactor));
    }

    if (s.view.zoomFactor != ZOOM_NONE)
    {
        s.scaledSelection = s.selection;
        s.scaledSelection.x *= s.view.zoomFactor;
        s.scaledSelection.y *= s.view.zoomFactor;
        s.scaledSelection.width *= s.view.zoomFactor;
        s.scaledSelection.height *= s.view.zoomFactor;

        if (!m_FitImageInWindow)
            m_ImageView->Scroll(zoomingCenter);
    }
    else
        m_ImageView->Refresh(true);

    UpdateWindowTitle();
 }

void c_MainWindow::CreateScaledPreview(bool eraseBackground)
{
    ProcessingSettings_t &s = *m_CurrentSettings;
    if (!s.m_ImgBmp)
        return;

    wxPoint scrollPos = m_ImageView->CalcUnscrolledPosition(wxPoint(0, 0));
    wxRect &sarea = s.view.scaledArea;
    sarea.SetLeft(scrollPos.x / s.view.zoomFactor);
    sarea.SetTop(scrollPos.y / s.view.zoomFactor);
    wxSize viewSize = m_ImageView->GetSize();
    sarea.SetWidth(viewSize.GetWidth() / s.view.zoomFactor);
    sarea.SetHeight(viewSize.GetHeight() / s.view.zoomFactor);

    // Limit the scaling request area to fit inside m_ImgBmp

    if (sarea.x < 0)
        sarea.x = 0;
    if (sarea.x >= s.m_ImgBmp->GetWidth())
        sarea.x = s.m_ImgBmp->GetWidth() - 1;
    if (sarea.GetRight() >= s.m_ImgBmp->GetWidth())
        sarea.SetRight(s.m_ImgBmp->GetWidth() - 1);

    if (sarea.y < 0)
        sarea.y = 0;
    if (sarea.y >= s.m_ImgBmp->GetHeight())
        sarea.y = s.m_ImgBmp->GetHeight() - 1;
    if (sarea.GetBottom() >= s.m_ImgBmp->GetHeight())
        sarea.SetBottom(s.m_ImgBmp->GetHeight() - 1);

    delete s.view.bmpScaled;
    wxBitmap srcBmp = s.m_ImgBmp->GetSubBitmap(sarea);
    s.view.bmpScaled = new wxBitmap(srcBmp.ConvertToImage().Scale(
        (int)srcBmp.GetWidth() * s.view.zoomFactor,
        (int)srcBmp.GetHeight() * s.view.zoomFactor,
        GetResizeQuality(s.scalingMethod)));

    m_ImageView->Refresh(eraseBackground);
}

void c_MainWindow::OnTimer(wxTimerEvent &event)
{
    switch (event.GetId())
    {
    case ID_ScalingTimer:
        {
            ProcessingSettings_t &s = *m_CurrentSettings;
            if (s.m_ImgBmp && s.view.zoomFactor != ZOOM_NONE)
            {
                CreateScaledPreview(s.view.zoomFactorChanged);
                s.view.zoomFactorChanged = false;
            }
            break;
        }
    }
}

void c_MainWindow::OnAuiPaneClose(wxAuiManagerEvent &event)
{
    // On wxWidgets 3.0.2 (wxGTK on Fedora 20), at this point the pane still returns IsShown() == true.
    // Workaround: update pane state manually.
    event.GetPane()->Hide();
    // End workaround

    UpdateToggleControlsState();
}

/// Updates state of menu items and toolbar buttons responsible for toggling the processing panel and tone curve editor
void c_MainWindow::UpdateToggleControlsState()
{
    bool processingPaneShown = m_AuiMgr.GetPane(PaneNames::processing).IsShown();
    GetMenuBar()->FindItem(ID_ToggleProcessingPanel)->Check(processingPaneShown);
    GetToolBar()->FindById(ID_ToggleProcessingPanel)->Toggle(processingPaneShown);

    bool tcrvEditShown = m_ToneCurveEditorWindow->IsShown();
    GetMenuBar()->FindItem(ID_ToggleToneCurveEditor)->Check(tcrvEditShown);
    GetToolBar()->FindById(ID_ToggleToneCurveEditor)->Toggle(tcrvEditShown);

    GetToolBar()->Realize();
}

void c_MainWindow::OnLucyRichardsonIters(wxSpinEvent &event)
{
    OnUpdateLucyRichardsonSettings();
}

void c_MainWindow::OnCloseToneCurveEditorWindow(wxCloseEvent &event)
{
    if (event.CanVeto())
    {
        Log::Print("Hiding tone curve editor\n");
        m_ToneCurveEditorWindow->Hide();
        UpdateToggleControlsState();
    }
    else
        event.Skip();
}

void c_MainWindow::OnToneCurveChanged(wxCommandEvent &event)
{
    if (m_CurrentSettings->m_Img)
        ScheduleProcessing(ProcessingRequest::TONE_CURVE);
}

/// Returns 'true' if sharpening settings have impact on the image
bool c_MainWindow::SharpeningEnabled()
{
    return (m_CurrentSettings->LucyRichardson.iterations > 0);
}

/// Returns 'true' if unsharp masking settings have impact on the image
bool c_MainWindow::UnshMaskingEnabled()
{
    ProcessingSettings_t &s = *m_CurrentSettings;
    return !s.UnsharpMasking.adaptive && s.UnsharpMasking.amountMax != 1.0f ||
           s.UnsharpMasking.adaptive && (s.UnsharpMasking.amountMin != 1.0f || s.UnsharpMasking.amountMax != 1.0f);
}

/// Returns 'true' if tone curve has impact on the image (i.e. it is not the identity map)
bool c_MainWindow::ToneCurveEnabled()
{
    c_ToneCurve &tc = m_CurrentSettings->toneCurve;
    return tc.IsGammaMode() && tc.GetGamma() != 1.0f ||
           tc.GetNumPoints() != 2 ||
           tc.GetPoint(0).x != 0.0f ||
           tc.GetPoint(0).y != 0.0f ||
           tc.GetPoint(1).x != 1.0f ||
           tc.GetPoint(1).y != 1.0f;
}

c_MainWindow::c_MainWindow()
{
    wxRect wndPos = Configuration::GetMainWindowPosSize();
    Create(NULL, wxID_ANY, _("ImPPG"), wndPos.GetTopLeft(), wndPos.GetSize());

    SetExtraStyle(GetExtraStyle() | wxWS_EX_VALIDATE_RECURSIVELY); // Make sure all validators are run

    m_CurrentSettings = new ProcessingSettings_t();
    ProcessingSettings_t &s = *m_CurrentSettings;

    s.normalization.enabled = false;
    s.normalization.min = 0.0;
    s.normalization.max = 1.0;

    s.LucyRichardson.sigma = Default::LR_SIGMA;
    s.LucyRichardson.iterations = Default::LR_ITERATIONS;
    s.LucyRichardson.deringing.enabled = false;

    s.UnsharpMasking.adaptive = false;
    s.UnsharpMasking.sigma = Default::UNSHMASK_SIGMA;
    s.UnsharpMasking.amountMin = Default::UNSHMASK_AMOUNT;
    s.UnsharpMasking.amountMax = Default::UNSHMASK_AMOUNT;
    s.UnsharpMasking.threshold = Default::UNSHMASK_THRESHOLD;
    s.UnsharpMasking.width = Default::UNSHMASK_WIDTH;

    m_Processing.processingScheduled = false;

    s.selection.x = s.selection.y = -1;
    s.selection.width = s.selection.height = 0;

    s.output.sharpening.img = 0;
    s.output.sharpening.valid = false;

    s.output.unsharpMasking.img = 0;
    s.output.unsharpMasking.valid = false;

    s.output.toneCurve.img = 0;
    s.output.toneCurve.valid = false;
    s.output.toneCurve.preciseValuesApplied = false;

    s.m_Img = 0;
    s.m_ImgBmp = 0;

    s.m_FileSaveScheduled = false;

    s.scalingMethod = S_CUBIC;

    s.view.zoomFactor = ZOOM_NONE;
    s.view.bmpScaled = 0;
    s.view.zoomFactorChanged = false;
    s.view.scalingTimer.SetOwner(this, ID_ScalingTimer);

    m_Processing.worker = 0;
    m_Processing.currentThreadId = 0;
    m_Processing.processingRequest = ProcessingRequest::NONE;
    m_Processing.usePreciseTCurveVals = false;

    m_MouseOps.dragging = false;
    m_MouseOps.dragScroll.dragging = false;

    m_FitImageInWindow = false;

    InitControls();
    SetStatusText(_("Idle"));
    if (Configuration::IsMainWindowMaximized())
        Maximize();

    FixWindowPosition(this);

    Show(true);
}

void c_MainWindow::OnProcessingStepCompleted(CompletionStatus_t status)
{
    SetActionText(_("Idle"));

    if (m_Processing.processingRequest == ProcessingRequest::TONE_CURVE ||
        status == RESULT_ABORTED)
    {
        if (m_Processing.processingRequest == ProcessingRequest::TONE_CURVE && status == RESULT_COMPLETED)
        {
            m_CurrentSettings->output.toneCurve.preciseValuesApplied = m_Processing.usePreciseTCurveVals;
        }

        // This flag is set only for saving the output file. Clear it if the TONE_CURVE processing request
        // has finished for any reason or there was an abort regardless of the current request.
        m_Processing.usePreciseTCurveVals = false;
    }

    if (status == RESULT_COMPLETED)
    {
        Log::Print("Processing step completed\n");

        bool allProcessingStepsFinished = false;

        if (m_Processing.processingRequest == ProcessingRequest::SHARPENING)
        {
            m_CurrentSettings->output.sharpening.valid = true;
            ScheduleProcessing(ProcessingRequest::UNSHARP_MASKING);
        }
        else if (m_Processing.processingRequest == ProcessingRequest::UNSHARP_MASKING)
        {
            m_CurrentSettings->output.unsharpMasking.valid = true;
            ScheduleProcessing(ProcessingRequest::TONE_CURVE);
        }
        else if (m_Processing.processingRequest == ProcessingRequest::TONE_CURVE)
        {
            m_CurrentSettings->output.toneCurve.valid = true;

            // All steps completed, draw the processed fragment
            UpdateSelectionAfterProcessing();
        }
    }
    else if (status == RESULT_ABORTED)
        m_CurrentSettings->m_FileSaveScheduled = false;
}

void c_MainWindow::OnThreadEvent(wxThreadEvent &event)
{
    // On rare occassions it may happen that the event is outdated and had been sent
    // by a previously launched worker thread, which has already deleted itself.
    // In such case, ignore the event.
    //
    // Otherwise, we would operate on (and delete!) on of the the 'output' images,
    // causing a crash, as the current worker thread (if present) could be writing to it.
    if (event.GetInt() != m_Processing.currentThreadId)
    {
        Log::Print(wxString::Format("Received an outdated event (%s) with threadId = %d\n",
                event.GetId() == ID_PROCESSING_PROGRESS ? "progress" : "completion", event.GetInt()));
        return;
    }

    switch (event.GetId())
    {
    case ID_PROCESSING_PROGRESS:
    {
        Log::Print(wxString::Format("Received a processing progress (%d%%) event from threadId = %d\n",
                event.GetPayload<WorkerEventPayload_t>().percentageComplete, event.GetInt()));

        wxString action;
        if (m_Processing.processingRequest == ProcessingRequest::SHARPENING)
            action = _(L"Lucy\u2013Richardson deconvolution");
        else if (m_Processing.processingRequest == ProcessingRequest::UNSHARP_MASKING)
            action = _("Unsharp masking");
        else if (m_Processing.processingRequest == ProcessingRequest::TONE_CURVE)
            action = _("Applying tone curve");

        SetActionText(wxString::Format(action + ": %d%%", event.GetPayload<WorkerEventPayload_t>().percentageComplete));
        break;
    }

    case ID_FINISHED_PROCESSING:
        {
            const WorkerEventPayload_t &p = event.GetPayload<WorkerEventPayload_t>();

            Log::Print(wxString::Format("Received a processing completion event from threadId = %d, status = %s\n",
                    event.GetInt(), p.completionStatus == RESULT_COMPLETED ? "COMPLETED" : "ABORTED"));

            OnProcessingStepCompleted(p.completionStatus);

            if (m_Processing.processingScheduled)
            {
                Log::Print("Waiting for the worker thread to finish... ");

                // Since we have just received the "finished processing" event, the worker thread will destroy itself any moment; keep polling
                while (IsProcessingInProgress())
                    wxThread::Yield();

                Log::Print("done\n");

                StartProcessing();
            }
            break;
        }
    }
}

/// Marks the selection's outline (using physical coords)
void c_MainWindow::MarkSelection(wxRect &selection, wxDC &dc)
{
#ifdef __WXMSW__

    wxRasterOperationMode oldMode = dc.GetLogicalFunction();
    dc.SetLogicalFunction(wxINVERT);
    dc.SetPen(*wxBLACK_PEN);
    dc.SetBrush(*wxTRANSPARENT_BRUSH);
    dc.DrawRectangle(selection);
    dc.SetLogicalFunction(oldMode);

#else
    // On other platforms, e.g. GTK 3 or OS X (but not GTK 2), logical DC operations are not supported.
    // To be on the safe side, draw the selection using a dashed pen instead.

    dc.SetBrush(*wxTRANSPARENT_BRUSH);

    wxPen pen1(*wxWHITE);
    dc.SetPen(pen1);
    dc.DrawRectangle(selection);

    wxPen pen2(*wxBLACK, 1, wxPENSTYLE_DOT);
    dc.SetPen(pen2);
    dc.DrawRectangle(selection);

#endif
}

void c_MainWindow::OnImageViewMouseDragStart(wxMouseEvent &event)
{
    if (m_CurrentSettings->m_Img)
    {
        m_MouseOps.dragging = true;

        m_MouseOps.View.start = event.GetPosition();
        m_MouseOps.View.end = m_MouseOps.View.start;

        ProcessingSettings_t &s = *m_CurrentSettings;

        if (s.view.zoomFactor == ZOOM_NONE)
        {
            m_MouseOps.dragStart = m_ImageView->CalcUnscrolledPosition(event.GetPosition());
        }
        else
        {
            m_MouseOps.dragStart = m_ImageView->CalcUnscrolledPosition(event.GetPosition());
            m_MouseOps.dragStart.x /= s.view.zoomFactor;
            m_MouseOps.dragStart.y /= s.view.zoomFactor;
        }

        m_MouseOps.dragEnd = m_MouseOps.dragStart;

        m_ImageView->CaptureMouse();

        m_MouseOps.prevSelectionBordersErased = false;
    }
}

void c_MainWindow::OnImageViewMouseMove(wxMouseEvent &event)
{
    m_ImageView->StopAutoScrolling();
    ProcessingSettings_t &s = *m_CurrentSettings;

    if (m_MouseOps.dragging)
    {
        if (s.view.zoomFactor == ZOOM_NONE)
            m_MouseOps.dragEnd = m_ImageView->CalcUnscrolledPosition(event.GetPosition());
        else
        {
            m_MouseOps.dragEnd = m_ImageView->CalcUnscrolledPosition(event.GetPosition());
            m_MouseOps.dragEnd.x /= s.view.zoomFactor;
            m_MouseOps.dragEnd.y /= s.view.zoomFactor;
        }

        // Erase the borders of the old selection
        if (!m_MouseOps.prevSelectionBordersErased)
        {
            wxRect physSelection;

            if (s.view.zoomFactor == ZOOM_NONE)
            {
                // Selection in physical (m_ImageView) coordinates
                physSelection = wxRect(
                    m_ImageView->CalcScrolledPosition(s.selection.GetTopLeft()),
                    m_ImageView->CalcScrolledPosition(s.selection.GetBottomRight()));
            }
            else
            {
                physSelection.SetTopLeft(m_ImageView->CalcScrolledPosition(s.scaledSelection.GetTopLeft()));
                physSelection.SetBottomRight(m_ImageView->CalcScrolledPosition(s.scaledSelection.GetBottomRight()));
            }

            m_ImageView->RefreshRect(wxRect(physSelection.GetLeft(), physSelection.GetTop(), physSelection.GetWidth(), 1),
                false);
            m_ImageView->RefreshRect(wxRect(physSelection.GetLeft(), physSelection.GetBottom(), physSelection.GetWidth(), 1),
                false);
            m_ImageView->RefreshRect(wxRect(physSelection.GetLeft(), physSelection.GetTop() + 1, 1, physSelection.GetHeight() - 2),
                false);
            m_ImageView->RefreshRect(wxRect(physSelection.GetRight(), physSelection.GetTop() + 1, 1, physSelection.GetHeight() - 2),
                false);

            m_MouseOps.prevSelectionBordersErased = true;
        }

        wxPoint selectionLimitMin = m_ImageView->CalcScrolledPosition(wxPoint(0, 0));
        wxPoint selectionLimitMax = m_ImageView->CalcScrolledPosition(wxPoint(
            (s.view.zoomFactor != ZOOM_NONE) ? s.m_ImgBmp->GetWidth() * s.view.zoomFactor : s.m_ImgBmp->GetWidth(),
            (s.view.zoomFactor != ZOOM_NONE) ? s.m_ImgBmp->GetHeight() * s.view.zoomFactor : s.m_ImgBmp->GetHeight()));

        // Erase the borders of the previous temporary selection (drawn during dragging)

        wxPoint oldSelTopLeft(std::min(m_MouseOps.View.start.x, m_MouseOps.View.end.x),
                              std::min(m_MouseOps.View.start.y, m_MouseOps.View.end.y));
        wxPoint oldSelBottomRight(std::max(m_MouseOps.View.start.x, m_MouseOps.View.end.x),
                                  std::max(m_MouseOps.View.start.y, m_MouseOps.View.end.y));
        int oldSelWidth = oldSelBottomRight.x - oldSelTopLeft.x + 1,
            oldSelHeight = oldSelBottomRight.y - oldSelTopLeft.y + 1;

        m_ImageView->RefreshRect(wxRect(oldSelTopLeft.x, oldSelTopLeft.y, oldSelWidth, 1),
             false);
        m_ImageView->RefreshRect(wxRect(oldSelTopLeft.x, oldSelBottomRight.y, oldSelWidth, 1),
            false);
        m_ImageView->RefreshRect(wxRect(oldSelTopLeft.x, oldSelTopLeft.y, 1, oldSelHeight),
            false);
        m_ImageView->RefreshRect(wxRect(oldSelBottomRight.x, oldSelTopLeft.y, 1, oldSelHeight),
            false);

        // Refresh the borders of the new selection

        m_MouseOps.View.end = event.GetPosition();

        if (m_MouseOps.View.end.x < selectionLimitMin.x)
            m_MouseOps.View.end.x = selectionLimitMin.x;
        if (m_MouseOps.View.end.x >= selectionLimitMax.x)
            m_MouseOps.View.end.x = selectionLimitMax.x - 1;

        if (m_MouseOps.View.end.y < selectionLimitMin.y)
            m_MouseOps.View.end.y = selectionLimitMin.y;
        if (m_MouseOps.View.end.y >= selectionLimitMax.y)
            m_MouseOps.View.end.y = selectionLimitMax.y - 1;

        wxPoint newSelTopLeft(std::min(m_MouseOps.View.start.x, m_MouseOps.View.end.x),
            std::min(m_MouseOps.View.start.y, m_MouseOps.View.end.y));
        wxPoint newSelBottomRight(std::max(m_MouseOps.View.start.x, m_MouseOps.View.end.x),
            std::max(m_MouseOps.View.start.y, m_MouseOps.View.end.y));
        int newSelWidth = newSelBottomRight.x - newSelTopLeft.x + 1,
            newSelHeight = newSelBottomRight.y - newSelTopLeft.y + 1;

        m_ImageView->RefreshRect(wxRect(newSelTopLeft.x, newSelTopLeft.y, newSelWidth, 1),
            false);
        m_ImageView->RefreshRect(wxRect(newSelTopLeft.x, newSelBottomRight.y, newSelWidth, 1),
            false);
        m_ImageView->RefreshRect(wxRect(newSelTopLeft.x, newSelTopLeft.y, 1, newSelHeight),
            false);
        m_ImageView->RefreshRect(wxRect(newSelBottomRight.x, newSelTopLeft.y, 1, newSelHeight),
            false);
    }
    else if (m_MouseOps.dragScroll.dragging)
    {
        wxPoint diff = m_MouseOps.dragScroll.start - event.GetPosition();
        wxPoint newScrollPos = m_MouseOps.dragScroll.startScrollPos + diff;
        m_ImageView->Scroll(newScrollPos);
        ScheduleScalingRequest();
    }
}

void c_MainWindow::OnImageViewMouseWheel(wxMouseEvent &event)
{
    ProcessingSettings_t &s = *m_CurrentSettings;

    wxPoint imgViewEvtPos; // event's position in m_ImageView's coordinates
    if (event.GetEventObject() == this)
        imgViewEvtPos = event.GetPosition() - m_ImageView->GetPosition();
    else
        imgViewEvtPos = event.GetPosition();

    if (s.m_ImgBmp && event.ControlDown() && m_ImageView->GetClientRect().Contains(imgViewEvtPos))
    {
        m_FitImageInWindow = false;

        float newZoom;

        if (event.GetWheelRotation() > 0)
            newZoom = CalcZoomIn(s.view.zoomFactor);
        else
            newZoom = CalcZoomOut(s.view.zoomFactor);


        ChangeZoom(newZoom, imgViewEvtPos);
    }
}

void c_MainWindow::OnImageViewMouseCaptureLost(wxMouseCaptureLostEvent &event)
{
    m_MouseOps.dragging = false;
    m_MouseOps.dragScroll.dragging = false;
}

void c_MainWindow::OnNewSelection(
        wxRect newSelection ///< Logical coordinates in the image
)
{
    ProcessingSettings_t &s = *m_CurrentSettings;

    Log::Print(wxString::Format("New selection at (%d, %d), w=%d, h=%d\n",
        newSelection.x, newSelection.y, newSelection.width, newSelection.height));

    // Restore unprocessed image contents in the previous selection
    boost::scoped_ptr<wxBitmap> restored(ImageToRgbBitmap(*s.m_Img, s.selection.x, s.selection.y, s.selection.width, s.selection.height));
    wxMemoryDC restoredDc(*restored);
    wxMemoryDC(*s.m_ImgBmp).Blit(s.selection.GetTopLeft(), s.selection.GetSize(), &restoredDc, wxPoint(0, 0));

    if (s.view.zoomFactor == ZOOM_NONE)
    {
        m_ImageView->RefreshRect(wxRect(
            m_ImageView->CalcScrolledPosition(s.selection.GetTopLeft()),
            m_ImageView->CalcScrolledPosition(s.selection.GetBottomRight())),
            false);
    }
    else if (s.view.bmpScaled)
    {
        // Restore also the corresponding fragment of the scaled bitmap.
        // Before restoring, increase the size of previous (unscaled) image fragment slightly to avoid any left-overs due to round-off errors
        int DELTA = std::max(6, (int)std::ceil(1.0f/s.view.zoomFactor));

        // Area in 'm_ImgBmp' to restore; based on 's.scaledSelection', but limited to what is currently visible.
        wxRect selectionRst;
        // First, take the scaled selection and limit it to visible area.
        selectionRst = s.scaledSelection;
        wxPoint scrollPos = m_ImageView->CalcUnscrolledPosition(wxPoint(0, 0));
        wxSize viewSize = m_ImageView->GetSize();

        selectionRst.Intersect(wxRect(scrollPos, viewSize));

        wxRect scaledSelectionRst = selectionRst; // Scaled area in 'm_ImgView' (logical coords) to restore

        // Second, scale it back to 'm_ImgBmp' pixels.
        selectionRst.x /= s.view.zoomFactor;
        selectionRst.y /= s.view.zoomFactor;
        selectionRst.width /= s.view.zoomFactor;
        selectionRst.height /= s.view.zoomFactor;

        selectionRst.Inflate(DELTA/2, DELTA/2);

        selectionRst.Intersect(wxRect(wxPoint(0, 0), s.m_ImgBmp->GetSize()));

        // Also expand the scaled area to restore
        int scaledSelectionDelta = (int)(s.view.zoomFactor * DELTA/2);
        scaledSelectionRst.Inflate(scaledSelectionDelta, scaledSelectionDelta);

        // Create the scaled image fragment to restore
        wxBitmap restoredScaled(s.m_ImgBmp->GetSubBitmap(selectionRst).ConvertToImage().Scale(
            scaledSelectionRst.GetWidth(), scaledSelectionRst.GetHeight(),
            GetResizeQuality(s.scalingMethod)));

        wxMemoryDC dcRestoredScaled(restoredScaled), dcScaled(*s.view.bmpScaled);

        wxPoint destPt = scaledSelectionRst.GetTopLeft();
        destPt.x -= s.view.scaledArea.x * s.view.zoomFactor;
        destPt.y -= s.view.scaledArea.y * s.view.zoomFactor;
        dcScaled.Blit(destPt, restoredScaled.GetSize(),
                &dcRestoredScaled,
                wxPoint(0, 0));

        wxRect updateReg(m_ImageView->CalcScrolledPosition(scaledSelectionRst.GetTopLeft()),
                         m_ImageView->CalcScrolledPosition(scaledSelectionRst.GetBottomRight()));
        m_ImageView->RefreshRect(updateReg, false);


        wxRect prevSelPhys(m_ImageView->CalcScrolledPosition(s.scaledSelection.GetTopLeft()),
                           m_ImageView->CalcScrolledPosition(s.scaledSelection.GetBottomRight()));
        m_ImageView->RefreshRect(wxRect(prevSelPhys.GetLeft(), prevSelPhys.GetTop(), prevSelPhys.GetWidth(), 1),
            false);
        m_ImageView->RefreshRect(wxRect(prevSelPhys.GetLeft(), prevSelPhys.GetBottom(), prevSelPhys.GetWidth(), 1),
            false);
        m_ImageView->RefreshRect(wxRect(prevSelPhys.GetLeft(), prevSelPhys.GetTop(), 1, prevSelPhys.GetHeight()),
            false);
        m_ImageView->RefreshRect(wxRect(prevSelPhys.GetRight(), prevSelPhys.GetTop(), 1, prevSelPhys.GetHeight()),
            false);


        s.scaledSelection = wxRect(m_ImageView->CalcUnscrolledPosition(m_MouseOps.View.start),
                                   m_ImageView->CalcUnscrolledPosition(m_MouseOps.View.end));
    }

    s.selection = newSelection;

    // Process the new selection, starting with sharpening
    ScheduleProcessing(ProcessingRequest::SHARPENING); //TODO: make this (auto-updating after selection changed) optional
}

void c_MainWindow::OnImageViewMouseDragEnd(wxMouseEvent &event)
{
    if (m_MouseOps.dragging)
    {
        m_MouseOps.dragging = false;
        m_ImageView->ReleaseMouse();

        if (m_MouseOps.dragStart != m_MouseOps.dragEnd)
            OnNewSelection(m_MouseOps.GetSelection(wxRect(0, 0,
                    m_CurrentSettings->m_Img->GetWidth(),
                    m_CurrentSettings->m_Img->GetHeight())));
    }
}

/// Sets text in the first field of the status bar
void c_MainWindow::SetActionText(wxString text)
{
    GetStatusBar()->SetStatusText(text, 0);
}

/// Returns the ratio of 'm_ImgView' to the size of 'imgSize', assuming uniform scaling in "touch from inside" fashion
float c_MainWindow::GetViewToImgRatio() const
{
    if ((float)m_ImageView->GetSize().GetWidth() / m_ImageView->GetSize().GetHeight() >
        (float)m_CurrentSettings->m_ImgBmp->GetWidth() / m_CurrentSettings->m_ImgBmp->GetHeight())
        return (float)m_ImageView->GetSize().GetHeight() / m_CurrentSettings->m_ImgBmp->GetHeight();
    else
        return (float)m_ImageView->GetSize().GetWidth() / m_CurrentSettings->m_ImgBmp->GetWidth();
}

void c_MainWindow::UpdateWindowTitle()
{
    ProcessingSettings_t &s = *m_CurrentSettings;
    SetTitle(wxString::Format(L"%s [%d%%] \u2013 ImPPG", s.inputFilePath, (int)(s.view.zoomFactor*100.0f))); // \u2013 is the N-dash
}

void c_MainWindow::OpenFile(wxFileName path, bool resetSelection)
{
    wxString ext = path.GetExt().Lower();

    std::string errorMsg;

    c_Image *newImg = LoadImageFileAsMono32f(path.GetFullPath().ToStdString(), path.GetExt().Lower().ToStdString(), &errorMsg);

    if (newImg == 0)
    {
        wxMessageBox(wxString::Format(_("Could not open %s."), path.GetFullPath()) + (errorMsg != "" ? "\n" + errorMsg : ""),
            _("Error"), wxICON_ERROR);
    }
    else
    {
        ProcessingSettings_t &s = *m_CurrentSettings;

        s.m_FileSaveScheduled = false;
        s.inputFilePath = path.GetFullPath();

        UpdateWindowTitle();

        if (s.m_Img)
        {
            delete s.m_Img;
        }
        s.m_Img = newImg;
        if (s.normalization.enabled)
            NormalizeFpImage(*s.m_Img, s.normalization.min, s.normalization.max);

        delete s.m_ImgBmp;
        s.m_ImgBmp = ImageToRgbBitmap(*s.m_Img, 0, 0,
            s.m_Img->GetWidth(),
            s.m_Img->GetHeight());

        if (resetSelection)
        {
            // Set initial selection to the middle 20% of the image
            s.selection.x = 4 * s.m_Img->GetWidth() / 10;
            s.selection.width = s.m_Img->GetWidth() / 5;
            s.selection.y = 4 * s.m_Img->GetHeight() / 10;
            s.selection.height = s.m_Img->GetHeight() / 5;

            s.scaledSelection = s.selection;
            s.scaledSelection.x *= s.view.zoomFactor;
            s.scaledSelection.y *= s.view.zoomFactor;
            s.scaledSelection.width *= s.view.zoomFactor;
            s.scaledSelection.height *= s.view.zoomFactor;
        }

        // Determine the selection's histogram and update the tone curve editor
        Histogram_t histogram;
        DetermineHistogram(*s.m_Img, s.selection, histogram);
        ((c_ToneCurveEditor *)FindWindowById(ID_ToneCurveEditor, this))->SetHistogram(histogram);

        // Initialize the images holding results of processing steps
        if (s.output.sharpening.img)
            delete s.output.sharpening.img;
        s.output.sharpening.img = new c_Image(s.selection.width, s.selection.height, PIX_MONO32F);
        c_Image::Copy(*s.m_Img, *s.output.sharpening.img, s.selection.x, s.selection.y,
            s.selection.width, s.selection.height, 0, 0);
        s.output.sharpening.valid = false;

        if (s.output.unsharpMasking.img)
            delete s.output.unsharpMasking.img;
        s.output.unsharpMasking.img = new c_Image(s.selection.width, s.selection.height, PIX_MONO32F);
        c_Image::Copy(*s.m_Img, *s.output.unsharpMasking.img, s.selection.x, s.selection.y,
            s.selection.width, s.selection.height, 0, 0);
        s.output.unsharpMasking.valid = false;

        if (s.output.toneCurve.img)
            delete s.output.toneCurve.img;
        s.output.toneCurve.img = new c_Image(s.selection.width, s.selection.height, PIX_MONO32F);
        c_Image::Copy(*s.m_Img, *s.output.toneCurve.img, s.selection.x, s.selection.y,
            s.selection.width, s.selection.height, 0, 0);
        s.output.toneCurve.valid = false;
        s.output.toneCurve.preciseValuesApplied = false;

        m_ImageView->SetVirtualSize(s.m_Img->GetWidth() * s.view.zoomFactor, s.m_Img->GetHeight() * s.view.zoomFactor);
        m_ImageView->SetScrollRate(1, 1);
        if (s.view.zoomFactor != ZOOM_NONE)
            CreateScaledPreview(true);
        else
            m_ImageView->Refresh(true);

        ScheduleProcessing(ProcessingRequest::SHARPENING);
    }
}

void c_MainWindow::OnOpenFile(wxCommandEvent &event)
{
    wxFileDialog dlg(this, _("Open image file"), Configuration::GetFileOpenPath(), "",
        INPUT_FILE_FILTERS, wxFD_OPEN | wxFD_FILE_MUST_EXIST);

    if (dlg.ShowModal() == wxID_OK)
    {
        Configuration::SetFileOpenPath(dlg.GetDirectory());
        wxFileName path = dlg.GetPath();
        OpenFile(path, true);
    }
}

void c_MainWindow::UpdateSelectionAfterProcessing()
{
    ProcessingSettings_t &s = *m_CurrentSettings;
    Log::Print("Updating selection after processing\n");

    boost::scoped_ptr<wxBitmap> updatedArea(ImageToRgbBitmap(*s.output.toneCurve.img, 0, 0,
        s.output.toneCurve.img->GetWidth(),
        s.output.toneCurve.img->GetHeight()));

    // Update the bitmap
    wxMemoryDC dcUpdated(*updatedArea), dcMain(*s.m_ImgBmp);
    dcMain.Blit(s.selection.GetTopLeft(), s.selection.GetSize(), &dcUpdated, wxPoint(0, 0));
    // 'updatedArea' needs to be deselected from DC before we can call GetSubBitmap() on it (see below)
    dcUpdated.SelectObject(wxNullBitmap);

    if (s.view.zoomFactor == ZOOM_NONE)
    {
        m_ImageView->RefreshRect(wxRect(
            m_ImageView->CalcScrolledPosition(s.selection.GetTopLeft()),
            m_ImageView->CalcScrolledPosition(s.selection.GetBottomRight())),
            false);
    }
    else if (s.view.bmpScaled)
    {
        // Area in 'updatedArea' to use; based on 's.scaledSelection', but limited to what is currently visible.
        wxRect selectionRst;
        // First, take the scaled selection and limit it to visible area.
        selectionRst = s.scaledSelection;
        wxPoint scrollPos = m_ImageView->CalcUnscrolledPosition(wxPoint(0, 0));
        wxSize viewSize = m_ImageView->GetSize();

        selectionRst.Intersect(wxRect(scrollPos, viewSize));

        wxRect scaledSelectionRst = selectionRst; // Scaled area in 'm_ImgView' (logical coords) to restore

        // Second, scale it back to 'm_ImgBmp' pixels.
        selectionRst.x /= s.view.zoomFactor;
        selectionRst.y /= s.view.zoomFactor;
        selectionRst.width /= s.view.zoomFactor;
        selectionRst.height /= s.view.zoomFactor;

        // Third, translate it from 'm_ImgBmp' to 'updatedArea' coordinates
        selectionRst.SetPosition(selectionRst.GetPosition() - s.selection.GetPosition());

        // Limit 'selectionRst' to fall within 'updatedArea'

        selectionRst.Intersect(wxRect(wxPoint(0, 0), updatedArea->GetSize()));

        // The user could have scrolled the view during processing, check if anything is visible
        if (selectionRst.GetWidth() == 0 || selectionRst.GetHeight() == 0)
            return;

        wxBitmap updatedAreaScaled(((updatedArea->GetSubBitmap(selectionRst).ConvertToImage().Scale(
            scaledSelectionRst.GetWidth(), scaledSelectionRst.GetHeight(),
            GetResizeQuality(s.scalingMethod)))));

        wxMemoryDC dcUpdatedScaled(updatedAreaScaled), dcScaled(*s.view.bmpScaled);

        wxPoint destPt = scaledSelectionRst.GetTopLeft();
        destPt.x -= s.view.scaledArea.x * s.view.zoomFactor;
        destPt.y -= s.view.scaledArea.y * s.view.zoomFactor;
        dcScaled.Blit(destPt, scaledSelectionRst.GetSize(),
                &dcUpdatedScaled,
                wxPoint(0, 0)/*FIXME: add origin of scaledSelectionRst*/);

        wxRect updateRegion(m_ImageView->CalcScrolledPosition(scaledSelectionRst.GetTopLeft()),
                            m_ImageView->CalcScrolledPosition(scaledSelectionRst.GetBottomRight()));
        m_ImageView->RefreshRect(updateRegion, false);
    }

    Histogram_t histogram;
    // Show histogram of the results of all processing steps up to unsharp masking,
    // but NOT including tone curve application.
    DetermineHistogram(*s.output.unsharpMasking.img, wxRect(0, 0, s.selection.width, s.selection.height), histogram);
    ((c_ToneCurveEditor *)FindWindowById(ID_ToneCurveEditor, this))->SetHistogram(histogram);

    if (s.m_FileSaveScheduled)
    {
        s.m_FileSaveScheduled = false;
        OnSaveFile();
    }
}

/// Returns 'true' if the processing thread is running
bool c_MainWindow::IsProcessingInProgress()
{
    wxCriticalSectionLocker lock(m_Processing.guard);
    return m_Processing.worker != 0;
}

/// Aborts processing and schedules new processing to start ASAP (as soon as 'm_Processing.worker' is not running)
void c_MainWindow::ScheduleProcessing(ProcessingRequest::ProcessingRequest_t request)
{
    ProcessingRequest::ProcessingRequest_t originalReq = request;

    // If the previous processing step(s) did not complete, we have to execute it (them) first

    if (request == ProcessingRequest::TONE_CURVE && !m_CurrentSettings->output.unsharpMasking.valid)
        request = ProcessingRequest::UNSHARP_MASKING;

    if (request == ProcessingRequest::UNSHARP_MASKING && !m_CurrentSettings->output.sharpening.valid)
        request = ProcessingRequest::SHARPENING;

    Log::Print(wxString::Format("Scheduling processing; requested: %d, scheduled: %d\n", originalReq, request));

    m_Processing.processingRequest = request;

    if (!IsProcessingInProgress())
        StartProcessing();
    else
    {
        // Signal (in a non-blocking way) the worker thread to finish ASAP.
        { wxCriticalSectionLocker lock(m_Processing.guard);

            if (m_Processing.worker)
            {
                Log::Print("Sending abort request to the worker thread\n");
                m_Processing.worker->AbortProcessing();
            }
        }

        // Set a flag so that we immediately restart the worker thread
        // after receiving the "processing finished" message.
        m_Processing.processingScheduled = true;
    }
}

/// Creates and starts a background processing thread
void c_MainWindow::StartProcessing()
{
    Log::Print("Starting processing\n");

    // Sanity check; the background thread should be finished and deleted at this point
    if (IsProcessingInProgress())
    {
        Log::Print("WARNING: The worker thread is still running!\n");
        return;
    }

    m_Processing.processingScheduled = false;

    ProcessingSettings_t &s = *m_CurrentSettings;

    // Make sure that if there are outdated thread events out there, they will be recognized
    // as such and discarded ('currentThreadId' will be sent from worker in event.GetInt()).
    // See also: c_MainWindow::OnThreadEvent()
    m_Processing.currentThreadId += 1;

    switch (m_Processing.processingRequest)
    {
    case ProcessingRequest::SHARPENING:
        if (s.output.sharpening.img)
            delete s.output.sharpening.img;

        s.output.sharpening.img = new c_Image(s.selection.width, s.selection.height, PIX_MONO32F);

        // Invalidate the current output and those of subsequent steps
        s.output.sharpening.valid = false;
        s.output.unsharpMasking.valid = false;
        s.output.toneCurve.valid = false;

        if (!SharpeningEnabled())
        {
            Log::Print("Sharpening disabled, no work needed\n");

            // No processing required, just copy the selection into 'output.sharpening.img',
            // as it will be used by the subsequent processing steps.

            c_Image::Copy(*s.m_Img, *s.output.sharpening.img,
                s.selection.x,
                s.selection.y,
                s.selection.width,
                s.selection.height,
                0, 0);
            OnProcessingStepCompleted(RESULT_COMPLETED);
        }
        else
        {
            Log::Print(wxString::Format("Launching L-R deconvolution worker thread (id = %d)\n",
                    m_Processing.currentThreadId));

            // Sharpening thread takes the currently selected fragment of the original image as input
            m_Processing.worker = new c_LucyRichardsonThread(
                *this, m_Processing.guard, &m_Processing.worker,
                0, // in the future we will pass the index of the currently open image
                new c_ImageBufferView(s.m_Img->GetBuffer(),
                    s.selection.x,
                    s.selection.y,
                    s.selection.width,
                    s.selection.height),
                s.output.sharpening.img->GetBuffer(),
                m_Processing.currentThreadId,
                s.LucyRichardson.sigma,
                s.LucyRichardson.iterations,
                s.LucyRichardson.deringing.enabled,
                254.0f/255, true, s.LucyRichardson.sigma
                );

            SetActionText(wxString::Format(_(L"L\u2013R deconvolution") + ": %d%%", 0));
            m_Processing.worker->Run();
        }

        break;

    case ProcessingRequest::UNSHARP_MASKING:

        if (s.output.unsharpMasking.img)
            delete s.output.unsharpMasking.img;

        s.output.unsharpMasking.img = new c_Image(s.selection.width, s.selection.height, PIX_MONO32F);

        // Invalidate the current output and those of subsequent steps
        s.output.unsharpMasking.valid = false;
        s.output.toneCurve.valid = false;

        if (!UnshMaskingEnabled())
        {
            Log::Print("Unsharp masking disabled, no work needed\n");

            // No processing required, just copy the selection into 'output.sharpening.img',
            // as it will be used by the subsequent processing steps.
            c_Image::Copy(*s.output.sharpening.img, *s.output.unsharpMasking.img, 0, 0, s.selection.width, s.selection.height, 0, 0);
            OnProcessingStepCompleted(RESULT_COMPLETED);
        }
        else
        {
            Log::Print(wxString::Format("Launching unsharp masking worker thread (id = %d)\n",
                    m_Processing.currentThreadId));

            // Unsharp masking thread takes the output of sharpening as input
            m_Processing.worker = new c_UnsharpMaskingThread(*this, m_Processing.guard, &m_Processing.worker,
                0, // in the future we will pass the index of currently open image
                new c_ImageBufferView(*s.output.sharpening.img),
                new c_ImageBufferView(s.m_Img->GetBuffer(), s.selection.x, s.selection.y, s.selection.width, s.selection.height),
                m_CurrentSettings->output.unsharpMasking.img->GetBuffer(),
                m_Processing.currentThreadId,
                s.UnsharpMasking.adaptive,
                s.UnsharpMasking.sigma,
                s.UnsharpMasking.amountMin,
                s.UnsharpMasking.amountMax,
                s.UnsharpMasking.threshold,
                s.UnsharpMasking.width);
            SetActionText(wxString::Format(_("Unsharp masking: %d%%"), 0));
            m_Processing.worker->Run();
        }
        break;

    case ProcessingRequest::TONE_CURVE:

        if (s.output.toneCurve.img)
            delete s.output.toneCurve.img;

        s.output.toneCurve.img = new c_Image(s.selection.width, s.selection.height, PIX_MONO32F);

        Log::Print("Created tone curve output image\n");

        // Invalidate the current output
        s.output.toneCurve.valid = false;

        if (!ToneCurveEnabled())
        {
            Log::Print("Tone curve is an identity map, no work needed\n");

            c_Image::Copy(*s.output.unsharpMasking.img , *s.output.toneCurve.img,
                0, 0, s.selection.width, s.selection.height, 0, 0);

            OnProcessingStepCompleted(RESULT_COMPLETED);
        }
        else
        {
            Log::Print(wxString::Format("Launching tone curve worker thread (id = %d)\n",
                    m_Processing.currentThreadId));

            // Tone curve thread takes the output of unsharp masking as input

            m_Processing.worker = new c_ToneCurveThread(*this, m_Processing.guard, &m_Processing.worker,
                0, // in the future we will pass the index of currently open image
                new c_ImageBufferView(*s.output.unsharpMasking.img),
                m_CurrentSettings->output.toneCurve.img->GetBuffer(),
                m_Processing.currentThreadId,
                m_CurrentSettings->toneCurve,
                m_Processing.usePreciseTCurveVals
                );
            SetActionText(wxString::Format(_("Applying tone curve: %d%%"), 0));
            m_Processing.worker->Run();
        }
        break;
    }
}

void c_MainWindow::OnUpdateLucyRichardsonSettings()
{
    if (m_CurrentSettings)
    {
        TransferDataFromWindow();

        m_CurrentSettings->LucyRichardson.iterations = ((wxSpinCtrl *)FindWindowById(ID_LucyRichardsonIters, this))->GetValue();
        m_CurrentSettings->LucyRichardson.sigma = ((c_NumericalCtrl *)FindWindowById(ID_LucyRichardsonSigma, this))->GetValue();
        m_CurrentSettings->LucyRichardson.deringing.enabled = ((wxCheckBox *)FindWindowById(ID_LucyRichardsonDeringing, this))->GetValue();

        if (m_CurrentSettings->m_Img)
            ScheduleProcessing(ProcessingRequest::SHARPENING);
    }
}

void c_MainWindow::OnUpdateUnsharpMaskingSettings()
{
    if (m_CurrentSettings)
    {
        TransferDataFromWindow();
        m_CurrentSettings->UnsharpMasking.sigma = ((c_NumericalCtrl *)FindWindowById(ID_UnsharpMaskingSigma, this))->GetValue();
        m_CurrentSettings->UnsharpMasking.amountMin = ((c_NumericalCtrl *)FindWindowById(ID_UnsharpMaskingAmountMin, this))->GetValue();
        m_CurrentSettings->UnsharpMasking.amountMax = ((c_NumericalCtrl *)FindWindowById(ID_UnsharpMaskingAmountMax, this))->GetValue();
        m_CurrentSettings->UnsharpMasking.threshold = ((c_NumericalCtrl *)FindWindowById(ID_UnsharpMaskingThreshold, this))->GetValue();
        m_CurrentSettings->UnsharpMasking.width = ((c_NumericalCtrl *)FindWindowById(ID_UnsharpMaskingWidth, this))->GetValue();

        if (m_CurrentSettings->m_Img)
            ScheduleProcessing(ProcessingRequest::UNSHARP_MASKING);
    }
}

float c_MainWindow::CalcZoomIn(float currentZoom)
{
    float newZoom = currentZoom * ZOOM_STEP;
    if (newZoom > ZOOM_MAX)
        newZoom = ZOOM_MAX;

    if (std::abs(newZoom - ZOOM_NONE) < 0.1f)
        newZoom = ZOOM_NONE;

    return newZoom;
}

float c_MainWindow::CalcZoomOut(float currentZoom)
{
    float newZoom = currentZoom / ZOOM_STEP;
    if (newZoom < ZOOM_MIN)
        newZoom = ZOOM_MIN;

    if (std::abs(newZoom - ZOOM_NONE) < 0.1f)
        newZoom = ZOOM_NONE;

    return newZoom;
}

void c_MainWindow::OnClose(wxCloseEvent &event)
{
    if (!IsMaximized())
        Configuration::SetMainWindowPosSize(wxRect(this->GetPosition(), this->GetSize()));
    Configuration::SetMainWindowMaximized(IsMaximized());
    Configuration::SetToneCurveEditorPosSize(
            wxRect(m_ToneCurveEditorWindow->GetPosition(),
                   m_ToneCurveEditorWindow->GetSize()));
    Configuration::SetToneCurveEditorVisible(m_ToneCurveEditorWindow->IsShown());
    Configuration::SetLogHistogram(((c_ToneCurveEditor *)FindWindowById(ID_ToneCurveEditor, this))->IsHistogramLogarithmic());
    Configuration::SetProcessingPanelWidth(FindWindowById(ID_ProcessingControlsPanel, this)->GetSize().GetWidth());

    // Signal the worker thread to finish ASAP.
    { wxCriticalSectionLocker lock(m_Processing.guard);
        if (m_Processing.worker)
        {
            Log::Print("Sending abort request to the worker thread\n");
            m_Processing.worker->AbortProcessing();
        }
    }
    while (IsProcessingInProgress())
        wxThread::Yield();

    event.Skip(); // Continue normal processing of this event
}

void c_MainWindow::SetUnsharpMaskingControlsVisibility()
{
    bool adaptiveEnabled = ((wxCheckBox *)FindWindowById(ID_UnsharpMaskingAdaptive, this))->IsChecked();

    ((c_NumericalCtrl *)FindWindowById(ID_UnsharpMaskingAmountMin))->Show(adaptiveEnabled);
    c_NumericalCtrl *ncAmountMax = (c_NumericalCtrl *)FindWindowById(ID_UnsharpMaskingAmountMax);
    if (adaptiveEnabled)
        ncAmountMax->SetLabel(_("Amount max:"));
    else
        ncAmountMax->SetLabel(_("Amount:"));
    ((c_NumericalCtrl *)FindWindowById(ID_UnsharpMaskingThreshold))->Show(adaptiveEnabled);
    ((c_NumericalCtrl *)FindWindowById(ID_UnsharpMaskingWidth))->Show(adaptiveEnabled);

    wxScrolledWindow *procPanel = (wxScrolledWindow *)FindWindowById(ID_ProcessingControlsPanel, this);
    procPanel->Layout();
    // As of wxWidgets 3.0.2, contrary to what documentation says, Layout() is not enough; in order for
    // procPanel to notice it needs to enable/disable scrollbars, we must call this:
    procPanel->SendSizeEvent();

    procPanel->Refresh(true);
}

void c_MainWindow::OnCommandEvent(wxCommandEvent &event)
{
    ProcessingSettings_t &s = *m_CurrentSettings;

    wxPoint imgViewMid(m_ImageView->GetSize().GetWidth()/2, m_ImageView->GetSize().GetHeight()/2);

    switch (event.GetId())
    {
    case wxID_EXIT: Close(); break;

    case ID_About:
        ShowAboutDialog(this);
        break;

    case ID_LucyRichardsonIters: // happens only if Enter pressed in the text control
    case ID_LucyRichardsonSigma:
    case ID_LucyRichardsonDeringing:
        OnUpdateLucyRichardsonSettings();
        break;

    case ID_LucyRichardsonReset:
        ((wxSpinCtrl *)FindWindowById(ID_LucyRichardsonIters, this))->SetValue(Default::LR_ITERATIONS);
        ((c_NumericalCtrl *)FindWindowById(ID_LucyRichardsonSigma, this))->SetValue(Default::LR_SIGMA);
        OnUpdateLucyRichardsonSettings();
        break;

    case ID_LucyRichardsonOff:
        ((wxSpinCtrl *)FindWindowById(ID_LucyRichardsonIters, this))->SetValue(0);
        OnUpdateLucyRichardsonSettings();
        break;

    case ID_UnsharpMaskingAdaptive:
    case ID_UnsharpMaskingSigma:
    case ID_UnsharpMaskingAmountMin:
    case ID_UnsharpMaskingAmountMax:
    case ID_UnsharpMaskingThreshold:
    case ID_UnsharpMaskingWidth:
        if (event.GetId() == ID_UnsharpMaskingAdaptive)
            SetUnsharpMaskingControlsVisibility();

        OnUpdateUnsharpMaskingSettings();
        break;

    case ID_UnsharpMaskingReset:
        ((wxCheckBox *)FindWindowById(ID_UnsharpMaskingAdaptive, this))->SetValue(false);

        ((c_NumericalCtrl *)FindWindowById(ID_UnsharpMaskingSigma, this))->SetValue(Default::UNSHMASK_SIGMA);
        ((c_NumericalCtrl *)FindWindowById(ID_UnsharpMaskingAmountMin, this))->SetValue(Default::UNSHMASK_AMOUNT);
        ((c_NumericalCtrl *)FindWindowById(ID_UnsharpMaskingAmountMax, this))->SetValue(Default::UNSHMASK_AMOUNT);
        ((c_NumericalCtrl *)FindWindowById(ID_UnsharpMaskingThreshold, this))->SetValue(Default::UNSHMASK_THRESHOLD);
        ((c_NumericalCtrl *)FindWindowById(ID_UnsharpMaskingWidth, this))->SetValue(Default::UNSHMASK_WIDTH);
        ((wxCheckBox *)FindWindowById(ID_UnsharpMaskingAdaptive, this))->SetValue(false);
        SetUnsharpMaskingControlsVisibility();
        OnUpdateUnsharpMaskingSettings();
        break;

    case ID_SelectAndProcessAll:
        if (s.m_Img)
        {
            // Set 'm_MouseOps' as if this new whole-image selection was marked with mouse by the user.
            // Needed for determining of 'scaledSelection' in OnNewSelection().
            m_MouseOps.View.start = m_ImageView->CalcScrolledPosition(wxPoint(0, 0));
            m_MouseOps.View.end = m_ImageView->CalcScrolledPosition(
                    wxPoint((int)s.m_ImgBmp->GetWidth() * s.view.zoomFactor,
                            (int)s.m_ImgBmp->GetHeight() * s.view.zoomFactor));

            OnNewSelection(wxRect(0, 0, s.m_Img->GetWidth(), s.m_Img->GetHeight()));
        }
        break;

    case ID_ToggleToneCurveEditor:
        m_ToneCurveEditorWindow->Show(!m_ToneCurveEditorWindow->IsShown());
        UpdateToggleControlsState();
        break;

    case ID_ToggleProcessingPanel:
        m_AuiMgr.GetPane(PaneNames::processing).Show(!m_AuiMgr.GetPane(PaneNames::processing).IsShown());
        m_AuiMgr.Update();
        UpdateToggleControlsState();
        break;

    case ID_FitInWindow:
        m_FitImageInWindow = !m_FitImageInWindow;
        // Surprisingly, we have to Freeze() here, because GetToolBar()->Realize()
        // forces an undesired, premature refresh
        m_ImageView->Freeze();
        GetToolBar()->FindById(ID_FitInWindow)->Toggle(m_FitImageInWindow);
        GetToolBar()->Realize();
        GetMenuBar()->FindItem(ID_FitInWindow)->Check(m_FitImageInWindow);

        if (s.m_Img)
        {
            if (m_FitImageInWindow)
                s.view.zoomFactor = GetViewToImgRatio();
            else
                s.view.zoomFactor = ZOOM_NONE;

            OnZoomChanged(wxPoint(0, 0));
            CreateScaledPreview(true);
        }
        m_ImageView->Thaw();
        break;

    case ID_SaveSettings:
        {
            wxFileDialog dlg(this, _("Save processing settings"), Configuration::GetSaveSettingsPath(), "",
                _("XML files (*.xml)") + "|*.xml", wxFD_SAVE);

            if (dlg.ShowModal() == wxID_OK)
            {
                Configuration::SetSaveSettingsPath(dlg.GetDirectory());

                wxFileName fname(dlg.GetPath());
                if (fname.GetExt() == wxEmptyString)
                    fname.SetExt("xml");

                if (!SaveSettings(fname.GetFullPath(),
                        s.LucyRichardson.sigma, s.LucyRichardson.iterations, s.LucyRichardson.deringing.enabled,
                        s.UnsharpMasking.adaptive, s.UnsharpMasking.sigma, s.UnsharpMasking.amountMin, s.UnsharpMasking.amountMax, s.UnsharpMasking.threshold, s.UnsharpMasking.width,
                        s.toneCurve, s.normalization.enabled, s.normalization.min, s.normalization.max))
                {
                    wxMessageBox(_("Failed to save processing settings."), _("Error"), wxOK | wxCENTRE | wxICON_ERROR);
                }
            }
            break;
        }

    case ID_LoadSettings:
        {
            wxFileDialog dlg(this, _("Load processing settings"), Configuration::GetLoadSettingsPath(), "",
                _("XML files (*.xml)") + "|*.xml", wxFD_OPEN | wxFD_FILE_MUST_EXIST);

            if (dlg.ShowModal() == wxID_OK)
            {
                Configuration::SetLoadSettingsPath(dlg.GetDirectory());

                if (!LoadSettings(dlg.GetPath(),
                      s.LucyRichardson.sigma, s.LucyRichardson.iterations, s.LucyRichardson.deringing.enabled,
                      s.UnsharpMasking.adaptive, s.UnsharpMasking.sigma, s.UnsharpMasking.amountMin, s.UnsharpMasking.amountMax, s.UnsharpMasking.threshold, s.UnsharpMasking.width,
                      s.toneCurve, s.normalization.enabled, s.normalization.min, s.normalization.max))
                {
                    wxMessageBox(_("Failed to load processing settings."), _("Error"), wxOK | wxCENTRE | wxICON_ERROR);
                }
                else
                {
                    ((c_NumericalCtrl *)FindWindowById(ID_LucyRichardsonSigma))->SetValue(s.LucyRichardson.sigma);
                    ((wxSpinCtrl *)FindWindowById(ID_LucyRichardsonIters))->SetValue(s.LucyRichardson.iterations);
                    ((wxCheckBox *)FindWindowById(ID_LucyRichardsonDeringing))->SetValue(s.LucyRichardson.deringing.enabled);

                    ((wxCheckBox *)FindWindowById(ID_UnsharpMaskingAdaptive))->SetValue(s.UnsharpMasking.adaptive);
                    ((c_NumericalCtrl *)FindWindowById(ID_UnsharpMaskingSigma))->SetValue(s.UnsharpMasking.sigma);
                    ((c_NumericalCtrl *)FindWindowById(ID_UnsharpMaskingAmountMin))->SetValue(s.UnsharpMasking.amountMin);
                    ((c_NumericalCtrl *)FindWindowById(ID_UnsharpMaskingAmountMax))->SetValue(s.UnsharpMasking.amountMax);
                    ((c_NumericalCtrl *)FindWindowById(ID_UnsharpMaskingThreshold))->SetValue(s.UnsharpMasking.threshold);
                    ((c_NumericalCtrl *)FindWindowById(ID_UnsharpMaskingWidth))->SetValue(s.UnsharpMasking.width);

                    ((c_ToneCurveEditor *)FindWindowById(ID_ToneCurveEditor, this))->SetToneCurve(&s.toneCurve);

                    OnUpdateLucyRichardsonSettings(); // Perform all processing steps, starting with L-R deconvolution
                }
            }
            break;
        }

    case wxID_SAVE:
        OnSaveFile();
        break;

    case ID_BatchProcessing:
        BatchProcessing(this);
        break;

    case ID_NormalizeImage:
        {
            c_NormalizeDialog dlg(this, s.normalization.enabled, s.normalization.min, s.normalization.max);
            if (dlg.ShowModal() == wxID_OK)
            {
                if (dlg.IsNormalizationEnabled())
                {
                    s.normalization.enabled = true;
                    s.normalization.min = dlg.GetMinLevel();
                    s.normalization.max = dlg.GetMaxLevel();
                }
                else
                    s.normalization.enabled = false;

                if (s.m_Img)
                {
                    // We don't keep the original non-normalized contents, so the file needs to be reloaded.
                    // Normalization using the new limits (if enabled) is performed by OpenFile().
                    OpenFile(wxFileName(s.inputFilePath), false);
                }
            }
        }
        break;

    case ID_ChooseLanguage:
        SelectLanguage();
        break;

    case ID_AlignImages:
        {
            AlignmentParameters_t params;

            if (GetAlignmentParameters(this, params))
                AlignImages(this, params);
        }
        break;

    case ID_ZoomIn: ChangeZoom(CalcZoomIn(s.view.zoomFactor), imgViewMid); break;

    case ID_ZoomOut: ChangeZoom(CalcZoomOut(s.view.zoomFactor), imgViewMid); break;

    case ID_Zoom33: ChangeZoom(1.0f/3.0f, imgViewMid); break;

    case ID_Zoom50: ChangeZoom(0.5f, imgViewMid); break;

    case ID_Zoom100: ChangeZoom(ZOOM_NONE, imgViewMid); break;

    case ID_Zoom150: ChangeZoom(1.5f, imgViewMid); break;

    case ID_Zoom200: ChangeZoom(2.0f, imgViewMid); break;

    case ID_ZoomCustom:
        {
            long percent = wxGetNumberFromUser(_("Enter zoom factor in %"), "", _("Custom zoom factor"), 100,
                    (long)(ZOOM_MIN * 100), (long)(ZOOM_MAX * 100), this);
            if (percent != -1)
                ChangeZoom(percent/100.0f, imgViewMid);
        }
        break;

    case ID_ScalingNearest:
        s.scalingMethod = S_NEAREST;
        CreateScaledPreview(true);
        break;

    case ID_ScalingLinear:
        s.scalingMethod = S_LINEAR;
        CreateScaledPreview(true);
        break;

    case ID_ScalingCubic:
        s.scalingMethod = S_CUBIC;
        CreateScaledPreview(true);
        break;
    }
}

wxPanel *c_MainWindow::CreateLucyRichardsonControlsPanel(wxWindow *parent)
{
    int maxfreq = Configuration::GetMaxProcessingRequestsPerSec();

    wxPanel *result = new wxPanel(parent);
    wxSizer *szTop = new wxBoxSizer(wxVERTICAL);

    wxSizer *szLRsigma = new wxBoxSizer(wxHORIZONTAL);

    szTop->Add(new c_NumericalCtrl(result, ID_LucyRichardsonSigma, _("Sigma:"), 0.5, 100.0,
        Default::LR_SIGMA, 0.05, 4, 2, 100, true, maxfreq ? 1000/maxfreq : 0),
        0, wxALIGN_CENTER_HORIZONTAL | wxGROW | wxALL, BORDER);

    wxSizer *szIters = new wxBoxSizer(wxHORIZONTAL);
    szIters->Add(new wxStaticText(result, wxID_ANY, _("Iterations:")), 0, wxALIGN_CENTER_VERTICAL | wxALL, BORDER);
    szIters->Add(new wxSpinCtrl(result, ID_LucyRichardsonIters, "", wxDefaultPosition, wxDefaultSize,
        wxTE_PROCESS_ENTER | wxSP_ARROW_KEYS| wxALIGN_RIGHT, 0, 500, Default::LR_ITERATIONS), 0, wxALIGN_CENTER_VERTICAL | wxALL, BORDER);
    FindWindowById(ID_LucyRichardsonIters, this)->SetToolTip(_(L"Suggested value: 30 to 70. Specify 0 to disable L\u2013R deconvolution."));
    szTop->Add(szIters, 0, wxALIGN_LEFT | wxALL, BORDER);

    szTop->Add(new wxCheckBox(result, ID_LucyRichardsonDeringing, _("Prevent ringing")), 0, wxALIGN_LEFT | wxALL, BORDER);
    FindWindowById(ID_LucyRichardsonDeringing, this)->SetToolTip(_("Prevents ringing (halo) around overexposed areas, e.g. a solar disc in a prominence image (experimental feature)."));

    wxSizer *szButtons = new wxBoxSizer(wxHORIZONTAL);
    szButtons->Add(new wxButton(result, ID_LucyRichardsonReset, _("reset"), wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT),
        0, wxALIGN_CENTER_VERTICAL | wxALL, BORDER);
    szButtons->Add(new wxButton(result, ID_LucyRichardsonOff, _("disable"), wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT),
        0, wxALIGN_CENTER_VERTICAL | wxALL, BORDER);

    szTop->Add(szButtons, 0, wxALIGN_LEFT | wxALL, BORDER);

    result->SetSizer(szTop);
    return result;
}

wxStaticBoxSizer *c_MainWindow::CreateUnsharpMaskingControls(wxWindow *parent)
{
    int maxfreq = Configuration::GetMaxProcessingRequestsPerSec();

    wxStaticBoxSizer *result = new wxStaticBoxSizer(wxVERTICAL, parent, _("Unsharp masking"));

    result->Add(new c_NumericalCtrl(
        (wxWindow *)result->GetStaticBox(), ID_UnsharpMaskingSigma,
        _("Sigma:"), 0.5, 200.0, Default::UNSHMASK_SIGMA, 0.05, 4, 2.0, 100, true, maxfreq ? 1000/maxfreq : 0),
        0, wxALIGN_CENTER_HORIZONTAL | wxGROW | wxALL, BORDER);

    c_NumericalCtrl *ncAmountMin = new c_NumericalCtrl((wxWindow *)result->GetStaticBox(), ID_UnsharpMaskingAmountMin,
            _("Amount min:"), 0, 100.0, Default::UNSHMASK_AMOUNT, 0.05, 4, 5.0, 100, true, maxfreq ? 1000/maxfreq : 0);
    ncAmountMin->SetToolTip(_("Value 1.0: no effect, <1.0: blur, >1.0: sharpen"));
    ncAmountMin->Show(false);
    result->Add(ncAmountMin, 0, wxALIGN_CENTER_HORIZONTAL | wxGROW | wxALL, BORDER);

    c_NumericalCtrl *ncAmountMax = new c_NumericalCtrl((wxWindow *)result->GetStaticBox(), ID_UnsharpMaskingAmountMax,
            _("Amount:"), 0, 100.0, Default::UNSHMASK_AMOUNT, 0.05, 4, 5.0, 100, true, maxfreq ? 1000/maxfreq : 0);
    ncAmountMax->SetToolTip(_("Value 1.0: no effect, <1.0: blur, >1.0: sharpen"));
    result->Add(ncAmountMax, 0, wxALIGN_CENTER_HORIZONTAL | wxGROW | wxALL, BORDER);

    c_NumericalCtrl *ncThreshold = new c_NumericalCtrl((wxWindow *)result->GetStaticBox(), ID_UnsharpMaskingThreshold,
            _("Threshold:"), 0, 1.0, Default::UNSHMASK_THRESHOLD, 0.05, 4, 5.0, 100, true, maxfreq ? 1000/maxfreq : 0);
    ncThreshold->SetToolTip(_("Input brightness threshold of transition from amount min to amount max"));
    ncThreshold->Show(false);
    result->Add(ncThreshold, 0, wxALIGN_CENTER_HORIZONTAL | wxGROW | wxALL, BORDER);

    c_NumericalCtrl *ncWidth = new c_NumericalCtrl((wxWindow *)result->GetStaticBox(), ID_UnsharpMaskingWidth,
            _("Transition width:"), 0, 1.0, Default::UNSHMASK_THRESHOLD, 0.05, 4, 5.0, 100, true, maxfreq ? 1000/maxfreq : 0);
    ncWidth->SetToolTip(_("Amount will be set to amount min for input brightness <= threshold-width and amount max for brightness >= threshold+width"));
    ncWidth->Show(false);
    result->Add(ncWidth, 0, wxALIGN_CENTER_HORIZONTAL | wxGROW | wxALL, BORDER);

    result->Add(new wxCheckBox((wxWindow *)result->GetStaticBox(), ID_UnsharpMaskingAdaptive, _("Adaptive"),
            wxDefaultPosition, wxDefaultSize, wxCHK_2STATE, wxGenericValidator(&m_CurrentSettings->UnsharpMasking.adaptive)),
            0, wxALIGN_LEFT | wxALL, BORDER);
    FindWindowById(ID_UnsharpMaskingAdaptive, this)->SetToolTip(_("Enable adaptive mode: amount changes from min to max depending on input brightness"));

    result->Add(new wxButton((wxWindow *)result->GetStaticBox(), ID_UnsharpMaskingReset, _("reset"), wxDefaultPosition,
        wxDefaultSize, wxBU_EXACTFIT), 0, wxALIGN_LEFT | wxALL, BORDER);

    return result;
}

/// Creates and returns a panel containing the processing controls
wxWindow *c_MainWindow::CreateProcessingControlsPanel()
{
    wxScrolledWindow *result = new wxScrolledWindow(this, ID_ProcessingControlsPanel);
    wxSizer *szTop = new wxBoxSizer(wxVERTICAL);

    wxNotebook *notebook = new wxNotebook(result, wxID_ANY);
    notebook->AddPage(CreateLucyRichardsonControlsPanel(notebook), _(L"Lucy\u2013Richardson deconvolution"), true);
    // ...
    // Add notebook pages with controls for other sharpening algorithms here.
    // ...
    szTop->Add(notebook, 0, wxALIGN_CENTER_HORIZONTAL | wxGROW | wxALL, BORDER);

    szTop->Add(CreateUnsharpMaskingControls(result), 0, wxALIGN_CENTER_HORIZONTAL | wxGROW | wxALL, BORDER);

    result->SetSizer(szTop);
    result->SetScrollRate(1, 1);

    result->Bind(wxEVT_SCROLLWIN_BOTTOM, &c_MainWindow::OnProcessingPanelScrolled, this);
    result->Bind(wxEVT_SCROLLWIN_LINEDOWN, &c_MainWindow::OnProcessingPanelScrolled, this);
    result->Bind(wxEVT_SCROLLWIN_LINEUP, &c_MainWindow::OnProcessingPanelScrolled, this);
    result->Bind(wxEVT_SCROLLWIN_PAGEDOWN, &c_MainWindow::OnProcessingPanelScrolled, this);
    result->Bind(wxEVT_SCROLLWIN_PAGEUP, &c_MainWindow::OnProcessingPanelScrolled, this);
    result->Bind(wxEVT_SCROLLWIN_THUMBRELEASE, &c_MainWindow::OnProcessingPanelScrolled, this);
    result->Bind(wxEVT_SCROLLWIN_THUMBTRACK, &c_MainWindow::OnProcessingPanelScrolled, this);
    result->Bind(wxEVT_SCROLLWIN_TOP, &c_MainWindow::OnProcessingPanelScrolled, this);

    return result;
}

/// Converts the specified fragment of 'src' to a 24-bit RGB bitmap
wxBitmap *c_MainWindow::ImageToRgbBitmap(c_Image &src, int x0, int y0, int width, int height)
{
    c_Image *rgbImage = c_Image::ConvertPixelFormat(src, PIX_RGB8, x0, y0, width, height);
    // For storage, 'rgbImage' uses c_SimpleBuffer, which has no row padding, so we can pass it directly to wxImage's constructor
    wxImage wximg(width, height, (unsigned char *)rgbImage->GetRow(0), true);
    wxBitmap *result = new wxBitmap(wximg);
    delete rgbImage;
    return result;
}

void c_MainWindow::OnPaintImageArea(wxPaintEvent &event)
{
    wxPaintDC dc(m_ImageView);
    wxRegionIterator upd(m_ImageView->GetUpdateRegion());
    ProcessingSettings_t &s = *m_CurrentSettings;

    if (s.m_ImgBmp)
    {
        wxRect currSel = m_MouseOps.dragging
            ? m_MouseOps.GetSelection(wxRect(0, 0,
                    s.m_Img->GetWidth(),
                    s.m_Img->GetHeight()))
            : s.selection;

        if (s.view.zoomFactor != ZOOM_NONE && s.view.bmpScaled)
        {
        /*
            PAINTING WHEN ZOOM <> 1.0

            Values of different variables and c_MainWindow members used in this method are illustrated below:


            +-----m_ImageView: virtual size (m_ImgBmp * zoomFactor) --------------------+
            |                                                                           |
            |                                                                           |
            |           +======= m_ImageView: visible portion ======================+   |
            |           |                                                           |   |
            |   +-------|---- updateArea (corresponds with s.view.bmpScaled) ----+  |   |
            |   |       |                                                        |  |   |
            |   |       |                                                        |  |   |
            |   |       |  +---- updRect -----+                                  |  |   |
            |   |       |  |                  |                                  |  |   |
            |   |       |  +------------------+                                  |  |   |
            |   |       +===========================================================+   |
            |   |                                                                |      |
            |   |                                                                |      |
            |   +----------------------------------------------------------------+      |
            |                                                                           |
            |                                                                           |
            |                                                                           |
            |                                                                           |
            +---------------------------------------------------------------------------+

            When we are asked to paint over 'updRect', we must blit from 's.view.bmpScaled' ('imgDC').
            This bitmap represents a scaled portion of 'm_ImgBmp', which does not necessarily correspond
            with the position of 'm_ImageView's' visible fragment at the moment. To find 'srcPt', which
            is the source point in 's.view.bmpScaled' to start blitting from, we must:
                - convert window (physical) left-top of 'updRect' to logical one within 'm_ImageView'
                - determine 'updateArea' by reverse-scaling 's.view.scaledArea'
                - express 'updRect' in 's.view.bmpScaled' logical coordinates (rather than m_ImageView)
                  by subtracting the left-top of 'updateArea' (which is expressed in m_ImageView logical coords)
        */

            wxMemoryDC imgDC(*s.view.bmpScaled);
            wxRect updateArea = s.view.scaledArea;
            updateArea.x *= s.view.zoomFactor;
            updateArea.y *= s.view.zoomFactor;
            updateArea.width *= s.view.zoomFactor;
            updateArea.height *= s.view.zoomFactor;

            while (upd)
            {
                wxRect updRect = upd.GetRect();
                wxPoint srcPt = m_ImageView->CalcUnscrolledPosition(updRect.GetTopLeft());
                srcPt.x -= updateArea.x;
                srcPt.y -= updateArea.y;
                dc.Blit(updRect.GetTopLeft(), updRect.GetSize(), &imgDC, srcPt);
                upd++;
            }

            // Selection in physical (m_ImageView) coordinates
            wxRect physSelection;
            if (m_MouseOps.dragging)
            {
                physSelection = wxRect(std::min(m_MouseOps.View.start.x, m_MouseOps.View.end.x),
                    std::min(m_MouseOps.View.start.y, m_MouseOps.View.end.y),
                    std::abs(m_MouseOps.View.end.x - m_MouseOps.View.start.x) + 1,
                    std::abs(m_MouseOps.View.end.y - m_MouseOps.View.start.y) + 1);
            }
            else
            {
                physSelection = wxRect(m_ImageView->CalcScrolledPosition(s.scaledSelection.GetTopLeft()),
                                       m_ImageView->CalcScrolledPosition(s.scaledSelection.GetBottomRight()));
            }

            MarkSelection(physSelection, dc);
        }
        else
        {
            wxMemoryDC imgDC(*s.m_ImgBmp);

            while (upd)
            {
                dc.Blit(wxPoint(upd.GetX(), upd.GetY()),
                    wxSize(upd.GetWidth(), upd.GetHeight()),
                    &imgDC, m_ImageView->CalcUnscrolledPosition(wxPoint(upd.GetX(), upd.GetY())));

                upd++;
            }

            // Selection in physical (m_ImageView) coordinates
            wxRect physSelection(
                m_ImageView->CalcScrolledPosition(currSel.GetTopLeft()),
                m_ImageView->CalcScrolledPosition(currSel.GetBottomRight()));
            MarkSelection(physSelection, dc);
        }
    }
}

void c_MainWindow::InitToolbar()
{
    CreateToolBar();
    wxToolBar *tb = GetToolBar();

    // File operations controls
	tb->AddTool(wxID_OPEN, wxEmptyString,
	        LoadBitmap("open_file"),
	        wxNullBitmap,
	        wxITEM_NORMAL,
	        _("Open image file"));
    tb->AddTool(wxID_SAVE, wxEmptyString,
        LoadBitmap("save_file"),
        wxNullBitmap,
        wxITEM_NORMAL,
        _("Save image file"));
    tb->AddSeparator();

    // User interface controls
    tb->AddCheckTool(ID_ToggleProcessingPanel, wxEmptyString, LoadBitmap("toggle_proc"),
        wxNullBitmap, _("Show processing controls"));
    tb->FindById(ID_ToggleProcessingPanel)->Toggle(true);

    tb->AddCheckTool(ID_ToggleToneCurveEditor, wxEmptyString, LoadBitmap("toggle_tcrv"),
        wxNullBitmap, _("Show tone curve editor"));

    tb->AddSeparator();

    // Processing controls
    tb->AddTool(ID_SelectAndProcessAll, wxEmptyString,
                LoadBitmap("select_all"),
                wxNullBitmap,
                wxITEM_NORMAL,
                _("Select and process the whole image"));

    tb->AddSeparator();

    // Zoom controls

    tb->AddCheckTool(ID_FitInWindow, wxEmptyString, LoadBitmap("fit_wnd"),
            wxNullBitmap, _("Fit image in window"));
    tb->AddTool(ID_Zoom100, wxEmptyString, LoadBitmap("zoom_none"),
            wxNullBitmap, wxITEM_NORMAL, _("Actual size (100%)"));
    tb->AddTool(ID_ZoomIn, wxEmptyString, LoadBitmap("zoom_in"),
            wxNullBitmap, wxITEM_NORMAL, _("Zoom in"));
    tb->AddTool(ID_ZoomOut, wxEmptyString, LoadBitmap("zoom_out"),
            wxNullBitmap, wxITEM_NORMAL, _("Zoom out"));
    tb->AddTool(ID_ZoomCustom, wxEmptyString, LoadBitmap("zoom_custom"),
            wxNullBitmap, wxITEM_NORMAL, _("Custom zoom factor..."));

    tb->AddSeparator();

    tb->AddTool(ID_SaveSettings, wxEmptyString, LoadBitmap("save_settings"),
            wxNullBitmap, wxITEM_NORMAL, _("Save processing settings"));
    tb->AddTool(ID_LoadSettings, wxEmptyString, LoadBitmap("load_settings"),
            wxNullBitmap, wxITEM_NORMAL, _("Load processing settings"));

    tb->Realize();
}

void c_MainWindow::InitMenu()
{
    wxMenu *menuFile = new wxMenu();
    menuFile->Append(wxID_OPEN);
    menuFile->Append(wxID_SAVE);
    menuFile->AppendSeparator();
    menuFile->Append(ID_LoadSettings, _("Load processing settings..."));
    menuFile->Append(ID_SaveSettings, _("Save processing settings..."));
    menuFile->AppendSeparator();
    menuFile->Append(ID_BatchProcessing, _("Batch processing..."));
    menuFile->AppendSeparator();
    menuFile->Append(wxID_EXIT);

    wxMenu *menuEdit = new wxMenu();
    menuEdit->Append(ID_SelectAndProcessAll, _("Select (and process) all\tCtrl+A"));

    wxMenu *menuSettings = new wxMenu();
    menuSettings->Append(ID_NormalizeImage, _("Normalize brightness levels..."), wxEmptyString, false);
    menuSettings->Append(ID_ChooseLanguage, _("Language..."), wxEmptyString, false);

    wxMenu *menuView = new wxMenu();
        wxMenu *menuPanels = new wxMenu();
        menuPanels->AppendCheckItem(ID_ToggleProcessingPanel, _("Processing settings"));
        menuPanels->AppendCheckItem(ID_ToggleToneCurveEditor, _("Tone curve"));
    menuView->AppendSubMenu(menuPanels, _("Panels"));
    menuView->AppendSeparator();
    menuView->AppendCheckItem(ID_FitInWindow, _("Fit image in window"));
    menuView->Append(ID_ZoomIn, _("Zoom in"));
    menuView->Append(ID_ZoomOut, _("Zoom out"));
    menuView->Append(ID_Zoom33, _("1:3 (33%)"));
    menuView->Append(ID_Zoom50, _("1:2 (50%)"));
    menuView->Append(ID_Zoom100, _("1:1 (100%)"));
    menuView->Append(ID_Zoom150, _("3:2 (150%)"));
    menuView->Append(ID_Zoom200, _("2:1 (200%)"));
    menuView->Append(ID_ZoomCustom, _("Custom zoom factor..."));

        wxMenu *menuScaling = new wxMenu();
        menuScaling->AppendRadioItem(ID_ScalingNearest, _("Nearest neighbor (fastest)"));
        menuScaling->AppendRadioItem(ID_ScalingLinear, _("Linear"));
        menuScaling->AppendRadioItem(ID_ScalingCubic, _("Cubic (best quality)"));
    menuView->AppendSubMenu(menuScaling, _("Scaling method"));

    wxMenu *menuTools = new wxMenu();
    menuTools->Append(ID_AlignImages, _("Align image sequence..."));

    // In theory, we could use just an "About" menu without items and react to its "on menu open" event.
    // In practice, it turns out that displaying a modal dialog (even a standard MessageBox) from
    // such an event's handler has undesired effects (as of wxWidgets 3.0.2): on Windows no application can be restored
    // and Win+D doesn't work; on wxGTK (Fedora 21+KDE) the screen doesn't react to any mouse events outside the dialog
    // and the dialog itself cannot be even moved or closed via its close box.
    //
    // So let's add a menu item and handle it in the usual way.
    wxMenu *menuAbout = new wxMenu();
        menuAbout->Append(ID_About, _("About ImPPG..."));

    wxMenuBar *menuBar = new wxMenuBar();
    menuBar->Append(menuFile, _("&File"));
    menuBar->Append(menuEdit, _("&Edit"));
    menuBar->Append(menuSettings, _("&Settings"));
    menuBar->Append(menuView, _("&View"));
    menuBar->Append(menuTools, _("&Tools"));
    menuBar->Append(menuAbout, _("About"));
    SetMenuBar(menuBar);

    GetMenuBar()->FindItem(ID_ToggleProcessingPanel)->Check();
    GetMenuBar()->FindItem(ID_ToggleToneCurveEditor)->Check();
    GetMenuBar()->FindItem(ID_ScalingCubic)->Check();
}

void c_MainWindow::InitStatusBar()
{
    CreateStatusBar(2);
    int fieldWidths[2] = { -1, -2 };
    GetStatusBar()->SetStatusWidths(2, fieldWidths);
}

void c_MainWindow::OnImageViewDragScrollStart(wxMouseEvent &event)
{
    if (m_CurrentSettings->m_Img)
    {
        m_MouseOps.dragScroll.dragging = true;
        m_MouseOps.dragScroll.start = event.GetPosition();
        m_MouseOps.dragScroll.startScrollPos = m_ImageView->CalcUnscrolledPosition(wxPoint(0, 0));
        m_ImageView->CaptureMouse();
        m_ImageView->SetCursor(wxCURSOR_SIZING);
    }
}

void c_MainWindow::OnImageViewDragScrollEnd(wxMouseEvent &event)
{
    if (m_MouseOps.dragScroll.dragging)
    {
        m_MouseOps.dragScroll.dragging = false;
        m_ImageView->ReleaseMouse();
        m_ImageView->SetCursor(wxCURSOR_CROSS);
    }
}

void c_MainWindow::InitControls()
{
    InitToolbar();
    InitStatusBar();
    InitMenu();

    m_AuiMgr.SetManagedWindow(this);

    wxWindow *processingPanel = CreateProcessingControlsPanel();

    wxSize procPanelSize = processingPanel->GetSizer()->GetMinSize();
    int procPanelSavedWidth = Configuration::GetProcessingPanelWidth();
    if (procPanelSavedWidth != -1)
        procPanelSize.SetWidth(procPanelSavedWidth);

    m_AuiMgr.AddPane(processingPanel,
        wxAuiPaneInfo()
        .Name(PaneNames::processing)
        .Caption(wxEmptyString)
        .Left()
        .CloseButton(true)
        .BottomDockable(false)
        .TopDockable(false)
        .PaneBorder()
        // Workaround to use the "min size" for display at first; if we started with BestSize(), it would be initially shown too small.
        // After the first call to Update() we relax the "min size", see below.
        .MinSize(procPanelSize)
        .BestSize(procPanelSize)
        );

    m_AuiMgr.Update();
    m_AuiMgr.GetPane(PaneNames::processing).MinSize(wxSize(1, 1));
    m_AuiMgr.Update();

    wxRect tcrvEditorPos = Configuration::GetToneCurveEditorPosSize();
    m_ToneCurveEditorWindow = new wxFrame(this, wxID_ANY, _("Tone curve"), tcrvEditorPos.GetTopLeft(), tcrvEditorPos.GetSize(),
        wxCAPTION | wxCLOSE_BOX | wxRESIZE_BORDER | wxFRAME_TOOL_WINDOW | wxFRAME_FLOAT_ON_PARENT);
    m_ToneCurveEditorWindow->SetSizer(new wxBoxSizer(wxHORIZONTAL));
    int maxfreq = Configuration::GetMaxProcessingRequestsPerSec();
    m_ToneCurveEditorWindow->GetSizer()->Add(
        new c_ToneCurveEditor(m_ToneCurveEditorWindow, &m_CurrentSettings->toneCurve, ID_ToneCurveEditor,
        maxfreq ? 1000 / maxfreq : 0, Configuration::GetLogHistogram()),
        1, wxGROW | wxALL);
    m_ToneCurveEditorWindow->Bind(wxEVT_CLOSE_WINDOW, &c_MainWindow::OnCloseToneCurveEditorWindow, this);
    if (tcrvEditorPos.GetSize().GetWidth() == -1) // Perform the initial Fit() only if previous position & size have not been loaded
        m_ToneCurveEditorWindow->Fit();
    if (Configuration::GetToneCurveEditorVisible())
        m_ToneCurveEditorWindow->Show();

    FixWindowPosition(m_ToneCurveEditorWindow);

    GetMenuBar()->FindItem(ID_ToggleToneCurveEditor)->Check(m_ToneCurveEditorWindow->IsShown());
    GetToolBar()->FindById(ID_ToggleToneCurveEditor)->Toggle(m_ToneCurveEditorWindow->IsShown());
    GetToolBar()->Realize();

    m_ImageView = new wxScrolledCanvas(this, ID_ImageView);
    m_ImageView->SetCursor(wxCURSOR_CROSS);

    m_ImageView->Bind(wxEVT_PAINT,              &c_MainWindow::OnPaintImageArea, this);
    m_ImageView->Bind(wxEVT_LEFT_DOWN,          &c_MainWindow::OnImageViewMouseDragStart, this);
    m_ImageView->Bind(wxEVT_MOTION,             &c_MainWindow::OnImageViewMouseMove, this);
    m_ImageView->Bind(wxEVT_LEFT_UP,            &c_MainWindow::OnImageViewMouseDragEnd, this);
    m_ImageView->Bind(wxEVT_MOUSE_CAPTURE_LOST, &c_MainWindow::OnImageViewMouseCaptureLost, this);
    m_ImageView->Bind(wxEVT_SIZE,               &c_MainWindow::OnImageViewSize, this);
    m_ImageView->Bind(wxEVT_MIDDLE_DOWN,        &c_MainWindow::OnImageViewDragScrollStart, this);
    m_ImageView->Bind(wxEVT_MIDDLE_UP,          &c_MainWindow::OnImageViewDragScrollEnd, this);
    m_ImageView->Bind(wxEVT_MOUSEWHEEL,         &c_MainWindow::OnImageViewMouseWheel, this);

    m_ImageView->Bind(wxEVT_SCROLLWIN_BOTTOM,   &c_MainWindow::OnImageViewScroll, this);
    m_ImageView->Bind(wxEVT_SCROLLWIN_LINEDOWN, &c_MainWindow::OnImageViewScroll, this);
    m_ImageView->Bind(wxEVT_SCROLLWIN_LINEUP,   &c_MainWindow::OnImageViewScroll, this);
    m_ImageView->Bind(wxEVT_SCROLLWIN_PAGEDOWN, &c_MainWindow::OnImageViewScroll, this);
    m_ImageView->Bind(wxEVT_SCROLLWIN_PAGEUP,   &c_MainWindow::OnImageViewScroll, this);
    m_ImageView->Bind(wxEVT_SCROLLWIN_THUMBTRACK, &c_MainWindow::OnImageViewScroll, this);
    m_ImageView->Bind(wxEVT_SCROLLWIN_TOP,      &c_MainWindow::OnImageViewScroll, this);

    m_AuiMgr.AddPane(m_ImageView,
        wxAuiPaneInfo()
        .Name(PaneNames::imageView)
        .Center()
        .Floatable(false)
        .CloseButton(false)
        .Gripper(false)
        .MinimizeButton(false)
        .PaneBorder(false)
        );

    m_AuiMgr.Update();
}

void c_MainWindow::ScheduleScalingRequest()
{
    ProcessingSettings_t &s = *m_CurrentSettings;
    if (s.m_Img)
    {
        if (s.view.zoomFactor != ZOOM_NONE)
            m_CurrentSettings->view.scalingTimer.StartOnce(IMAGE_SCALING_DELAY);
    }
}

void c_MainWindow::OnImageViewSize(wxSizeEvent &event)
{
    if (m_FitImageInWindow && m_CurrentSettings->m_ImgBmp)
    {
        m_CurrentSettings->view.zoomFactor = GetViewToImgRatio();
        m_CurrentSettings->view.zoomFactorChanged = true;
        OnZoomChanged(wxPoint(0, 0));
    }

    ScheduleScalingRequest();
    event.Skip();
}

c_MainWindow::~c_MainWindow()
{
    m_AuiMgr.UnInit();
    delete m_CurrentSettings->m_Img;
    delete m_CurrentSettings->m_ImgBmp;
    delete m_CurrentSettings->output.sharpening.img;
    delete m_CurrentSettings->output.unsharpMasking.img;
    delete m_CurrentSettings->output.toneCurve.img;
    delete m_CurrentSettings->view.bmpScaled;
    delete m_CurrentSettings;
}

/// Determines histogram of the specified area of an image
void c_MainWindow::DetermineHistogram(c_Image &img, const wxRect &selection, Histogram_t &histogram)
{
    histogram.values.clear();
    histogram.values.insert(histogram.values.begin(), NUM_HISTOGRAM_BINS, 0);
    histogram.minValue = FLT_MAX;
    histogram.maxValue = -FLT_MAX;

    for (int y = 0; y < selection.height; y++)
    {
        float *row = (float *)img.GetRow(selection.y + y) + selection.x;
        for (int x = 0; x < selection.width; x++)
        {
            if (row[x] < histogram.minValue)
                histogram.minValue = row[x];
            else if (row[x] > histogram.maxValue)
                histogram.maxValue = row[x];

            histogram.values[(int)(row[x] * (NUM_HISTOGRAM_BINS - 1))] += 1;
        }
    }

    histogram.maxCount = 0;
    for (unsigned i = 0; i < histogram.values.size(); i++)
        if (histogram.values[i] > histogram.maxCount)
            histogram.maxCount = histogram.values[i];
}

void c_MainWindow::OnProcessingPanelScrolled(wxScrollWinEvent &event)
{
    // As of wxWidgets 3.0.2, sometimes some child controls remain unrefreshed (graphically corrupted), so refresh everything
    FindWindowById(ID_ProcessingControlsPanel)->Refresh(false);
}
