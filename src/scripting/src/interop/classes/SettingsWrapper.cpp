#include "interop/classes/SettingsWrapper.h"

namespace scripting
{

const ProcessingSettings& SettingsWrapper::GetSettings() const
{
    return m_SettingsWrapper;
}

}
