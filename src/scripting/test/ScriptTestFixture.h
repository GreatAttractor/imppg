#pragma once

#include "common/proc_settings.h"
#include "scripting/script_image_processor.h"

#include <filesystem>
#include <future>
#include <initializer_list>
#include <memory>
#include <unordered_map>
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

    const std::vector<std::string>& GetStringNotifications() const { return m_StringNotifications; }

private:
    void OnIdle(wxIdleEvent& event);
    void OnRunnerMessage(wxThreadEvent& event);
    void OnScriptMessageContents(scripting::ScriptMessagePayload& payload);

    std::unique_ptr<scripting::ScriptImageProcessor> m_Processor;
    std::unique_ptr<wxAppConsole> m_App;
    std::promise<void> m_StopScript;
    std::vector<std::filesystem::path> m_TemporaryFiles;
    // value: occurrence count
    std::unordered_map<std::string, std::size_t> m_StringNotificationsUnordered;
    std::vector<std::string> m_StringNotifications;
    std::optional<ProcessingSettings> m_SettingsNotification;
    std::shared_ptr<const c_Image> m_ImageNotification;
    std::vector<double> m_NumberNotifications;
    std::vector<bool> m_BooleanNotifications;
    std::vector<int> m_IntegerNotifications;
    bool m_ScriptExecutionFailure{false};
};
