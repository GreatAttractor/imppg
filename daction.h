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
Delayed action interface header.
*/


#ifndef IMPPG_DELAYED_ACTION_HEADER
#define IMPPG_DELAYED_ACTION_HEADER

#include <wx/timer.h>
#include <wx/datetime.h>

/// Performs the action specified by DoPerformAction(), but no more often than every 'm_DelayMsec' milliseconds
class IDelayedAction
{
    /// Delay between subsequent calls to DoPerformAction()
    unsigned m_DelayMsec;
    
    /// Time of the last call to DoPerformAction()
    wxDateTime m_LastActionTimestamp;

    wxTimer *m_DelayedActionTimer;

    void *m_NextActionParam;

    void OnTimer(wxTimerEvent &event)
    {
        m_LastActionTimestamp = wxDateTime::UNow();
        DoPerformAction(m_NextActionParam);
        m_NextActionParam = 0;
    }

protected:
    /// Action implementation, to be provided by the derived classes
    virtual void DoPerformAction(void *param = 0) = 0;

public:

    IDelayedAction()
    {
        m_DelayMsec = 0;
        m_DelayedActionTimer = new wxTimer();
        m_LastActionTimestamp = wxDateTime::UNow();
        m_DelayedActionTimer->Bind(wxEVT_TIMER, &IDelayedAction::OnTimer, this);
    }

    ~IDelayedAction() { delete m_DelayedActionTimer; }

    void SetActionDelay(unsigned msec) { m_DelayMsec = msec; }
    unsigned SetActionDelay()          { return m_DelayMsec; }

    /// Makes sure that 'm_DelayMsec' have elapsed since the last call and performs the action implemented in DoPerformAction()
    /** Calls are not "buffered". If DelayedAction() is called more than once before 'm_DelayMsec' elapses, only the last call will be effective. */
    void DelayedAction(void *param = 0)
    {
        wxDateTime now = wxDateTime::UNow();
        unsigned msecSinceLastEvent = (unsigned)now.Subtract(m_LastActionTimestamp).GetMilliseconds().GetValue();
        if (msecSinceLastEvent > m_DelayMsec)
        {
            m_DelayedActionTimer->Stop();
            m_NextActionParam = 0;
            m_LastActionTimestamp = now;
            DoPerformAction(param);
        }
        else
        {
            m_NextActionParam = param;
            m_DelayedActionTimer->StartOnce(m_DelayMsec);
        }
    }
};


#endif // IMPPG_DELAYED_ACTION_HEADER
