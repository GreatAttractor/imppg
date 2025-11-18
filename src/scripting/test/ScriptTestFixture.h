/*
ImPPG (Image Post-Processor) - common operations for astronomical stacks and other images
Copyright (C) 2023-2025 Filip Szczerek <ga.software@yahoo.com>

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
    Script test fixture header.
*/

#pragma once

#include "common/proc_settings.h"
#include "scripting/script_image_processor.h"

#include <wx/hashmap.h>

#include <filesystem>
#include <future>
#include <initializer_list>
#include <memory>
#include <vector>

namespace scripting { class ScriptMessagePayload; }
class c_Image;
class wxAppConsole;
class wxThreadEvent;

std::filesystem::path GetTestRoot();

class ScriptTestFixture
{
public:
    ScriptTestFixture();

    ~ScriptTestFixture();

    /// Returns false on script execution error.
    bool RunScript(const char* scriptText);

    /// Creates file which is deleted on ScriptTestFixture destruction.
    void CreateEmptyFile(const std::filesystem::path& path);

    void CheckStringNotifications(std::initializer_list<std::string> expected) const;

    void CheckUnorderedStringNotifications(std::initializer_list<std::string> expected) const;

    const ProcessingSettings& GetSettingsNotification() const;

    const c_Image& GetImageNotification() const;

    void CheckNumberNotifications(std::initializer_list<double> expected) const;

    void CheckBooleanNotifications(std::initializer_list<bool> expected) const;

    void CheckIntegerNotifications(std::initializer_list<int> expected) const;

    const std::vector<wxString>& GetStringNotifications() const { return m_StringNotifications; }

private:
    WX_DECLARE_STRING_HASH_MAP(std::size_t, StringNotificationsMap);

    void OnIdle(wxIdleEvent& event);
    void OnRunnerMessage(wxThreadEvent& event);
    void OnScriptMessageContents(scripting::ScriptMessagePayload& payload);

    std::unique_ptr<scripting::ScriptImageProcessor> m_Processor;
    std::unique_ptr<wxAppConsole> m_App;
    std::promise<void> m_StopScript;
    std::vector<std::filesystem::path> m_TemporaryFiles;
    // value: occurrence count
    StringNotificationsMap m_StringNotificationsUnordered;
    std::vector<wxString> m_StringNotifications;
    std::optional<ProcessingSettings> m_SettingsNotification;
    std::shared_ptr<const c_Image> m_ImageNotification;
    std::vector<double> m_NumberNotifications;
    std::vector<bool> m_BooleanNotifications;
    std::vector<int> m_IntegerNotifications;
    bool m_ScriptExecutionFailure{false};
};
