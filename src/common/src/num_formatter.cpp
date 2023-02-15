/*
ImPPG (Image Post-Processor) - common operations for astronomical stacks and other images
Copyright (C) 2016-2022 Filip Szczerek <ga.software@yahoo.com>

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
    Numerical value formatter for storing/loading processing settings.

    As of wxWidgets 3.1.5, `wxString::FromCDouble` behaves incorrectly for certain combinations of system language
    and system regional settings (e.g., in MS Windows 10 with system language "US English" and regional settings
    "Italian") after a `wxLocale::Init` call. To make sure `.` is always used as the decimal separator, we format
    and parse using an at-program-start-default-constructed `std::stringstream`.

*/

#include "common/num_formatter.h"

#include <iomanip>
#include <sstream>


namespace NumFormatter
{

/// Default-constructed stream using "classic" locale (unless someone calls `std::locale::global` in a constructor of
/// another static object...)
static std::stringstream stream;

wxString Format(double value, unsigned numDecimals)
{
    stream.clear();
    stream.str("");
    stream << std::fixed << std::setprecision(numDecimals) << value;
    std::string s;
    stream >> s;
    return wxString(std::move(s));
}

bool Parse(wxString s, float& value)
{
    s.Replace(",", ".");
    stream.clear();
    stream.str(s.ToStdString());
    stream >> value;
    return !stream.fail() && stream.eof();
}

bool ParseList(wxString s, std::vector<float>& values, char separator)
{
    s.Replace(",", ".");

    values.clear();
    stream.clear();
    stream.str(s.ToStdString());
    while (true)
    {
        float value{};

        const auto posBefore = [&] {
            const auto pos = stream.tellg();
            if (pos == static_cast<std::streamoff>(s.size())) // happens if `s` contains a trailing separator
            {
                return decltype(stream)::pos_type(-1);
            }
            else
            {
                return pos;
            }
        }();
        stream >> value;
        const bool parsingFailed = stream.fail();
        const auto posAfter = stream.tellg(); // note: sets failbit if stream has been exhausted
        const bool charsConsumed = (posBefore != posAfter);

        if (parsingFailed && charsConsumed)
        {
            return false;
        }
        else if (stream.eof() && !charsConsumed)
        {
            break;
        }
        else
        {
            values.push_back(value);
        }

        char sep;
        stream >> sep;

        if (stream.eof())
        {
            break;
        }

        if (sep != separator)
        {
            return false;
        }
    }

    return true;
}

}
