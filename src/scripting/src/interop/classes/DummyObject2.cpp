#include "interop/classes/DummyObject2.h"

namespace scripting
{

DummyObject2::DummyObject2(int i): m_I(i)
{}

int DummyObject2::get(int n) const { return m_I + n; }

}
