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
    Exclusive object access class.
*/

#ifndef IMPPG_EXCLUSIVE_ACCESS_H
#define IMPPG_EXCLUSIVE_ACCESS_H

#include <memory>
#include <wx/thread.h>

/// As of wxWidgets 3.0.4, wxCriticalSection is non-movable; this is a movable wrapper.
class MovableCriticalSection
{
public:
    std::unique_ptr<wxCriticalSection> cs{ new wxCriticalSection() };
};

template<typename T>
class AccessGuard
{
    T& m_GuardedObj;
    wxCriticalSection& m_Guard;

public:
    AccessGuard(T& guardedObj, wxCriticalSection& guard): m_GuardedObj(guardedObj), m_Guard(guard)
    {
        m_Guard.Enter();
    }

    ~AccessGuard() { m_Guard.Leave(); }

    T& Get() { return m_GuardedObj; }
};

template<typename T>
class ExclusiveAccessObject
{
    T m_GuardedObject;
    MovableCriticalSection m_Guard;

public:
    ExclusiveAccessObject(T&& guardedObj): m_GuardedObject(std::move(guardedObj)) {}

    ExclusiveAccessObject(const ExclusiveAccessObject& rhs) = delete;
    ExclusiveAccessObject& operator=(const ExclusiveAccessObject&) = delete;
    ExclusiveAccessObject(ExclusiveAccessObject&& rhs) = default;
    ExclusiveAccessObject& operator=(ExclusiveAccessObject&&) = default;

    AccessGuard<T> Lock() { return AccessGuard(m_GuardedObject, *m_Guard.cs); }
};

#endif // IMPPG_EXCLUSIVE_ACCESS_H