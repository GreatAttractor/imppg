#pragma once

#include <filesystem>
#include <future>
#include <initializer_list>
#include <memory>
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

    /// Returns false on script execution error.
    bool RunScript(const char* scriptText);

    /// Creates file which is deleted on ScriptTestFixture destruction.
    void CreateFile(const std::filesystem::path& path);

    //const std::vector<std::string>& GetStringNotifications() const;

    void CheckStringNotifications(std::initializer_list<std::string> expected) const;

    const ProcessingSettings& GetSettingsNotification() const;

    const c_Image& GetImageNotification() const;

    void CheckNumberNotifications(std::initializer_list<double> expected) const;

    void CheckBooleanNotifications(std::initializer_list<bool> expected) const;

    void CheckIntegerNotifications(std::initializer_list<int> expected) const;

    template<typename Value>
    static void FillImage(c_Image& image, Value value, std::size_t channel = 0)
    {
        //
    }

private:
    void OnRunnerMessage(wxThreadEvent& event);
    void OnScriptFunctionCall(scripting::ScriptMessagePayload& payload);

    std::unique_ptr<wxAppConsole> m_App;
    std::promise<void> m_StopScript;
    std::vector<std::filesystem::path> m_TemporaryFiles;
    // value: occurrence count
    std::unordered_map<std::string, std::size_t> m_StringNotifications;
    std::shared_ptr<ProcessingSettings> m_SettingsNotification;
    std::shared_ptr<c_Image> m_ImageNotification;
    std::vector<double> m_NumberNotifications;
    std::vector<bool> m_BooleanNotifications;
    std::vector<int> m_IntegerNotifications;
    bool m_ScriptExecutionFailure{false};
};
