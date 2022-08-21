#pragma once

#include <filesystem>
#include <future>
#include <initializer_list>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

#include "common/proc_settings.h"
#include "image/image.h"

namespace scripting { class ScriptMessagePayload; }
class wxAppConsole;
class wxThreadEvent;

class ScriptTestFixture
{
public:
    ScriptTestFixture();

    ~ScriptTestFixture();

    void RunScript(const char* scriptText);

    /// Creates file which is deleted on ScriptTestFixture destruction.
    void CreateFile(const std::filesystem::path& path);

    //const std::vector<std::string>& GetStringNotifications() const;

    void CheckStringNotifications(std::initializer_list<std::string> expected) const;

    const ProcessingSettings& GetSettingsNotification() const;

private:
    void OnRunnerMessage(wxThreadEvent& event);
    void OnScriptFunctionCall(scripting::ScriptMessagePayload& payload);

    std::unique_ptr<wxAppConsole> m_App;
    std::promise<void> m_StopScript;
    std::vector<std::filesystem::path> m_TemporaryFiles;
    // value: occurrence count
    std::unordered_map<std::string, std::size_t> m_StringNotifications;
    std::optional<ProcessingSettings> m_SettingsNotification;
    std::optional<c_Image> m_ImageNotification;
};
