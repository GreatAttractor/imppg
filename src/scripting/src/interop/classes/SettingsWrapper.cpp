#include "interop/classes/SettingsWrapper.h"

namespace scripting
{

//int DummyObject1::get() const { return 55; }

ProcessingSettings SettingsWrapper::GetSettings() const
{
    return m_SettingsWrapper;
}

}
