/*
ImPPG (Image Post-Processor) - common operations for astronomical stacks and other images
Copyright (C) 2025 Filip Szczerek <ga.software@yahoo.com>

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
    Tone curve unit tests.
*/

#include "common/tcrv.h"

#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_CASE(GetNeighborsCase1)
{
    c_ToneCurve tc;
    tc.AddPoint(0.5, 1.0);

    BOOST_CHECK(std::make_pair(0.0f, 0.5f) == tc.GetNeighbors(0.0));
    BOOST_CHECK(std::make_pair(0.0f, 0.5f) == tc.GetNeighbors(0.25));
    BOOST_CHECK(std::make_pair(0.5f, 1.0f) == tc.GetNeighbors(0.5));
    BOOST_CHECK(std::make_pair(0.5f, 1.0f) == tc.GetNeighbors(0.75));
    BOOST_CHECK(std::make_pair(0.5f, 1.0f) == tc.GetNeighbors(1.0));
}

BOOST_AUTO_TEST_CASE(GetNeighborsCase2)
{
    c_ToneCurve tc;
    tc.UpdatePoint(0, 0.25, 0.0);
    tc.UpdatePoint(1, 0.75, 0.0);
    tc.AddPoint(0.5, 1.0);

    BOOST_CHECK(std::make_pair(0.0f, 0.25f) == tc.GetNeighbors(0.0f));
    BOOST_CHECK(std::make_pair(0.0f, 0.25f) == tc.GetNeighbors(0.1f));
    BOOST_CHECK(std::make_pair(0.25f, 0.5f) == tc.GetNeighbors(0.25f));
    BOOST_CHECK(std::make_pair(0.5f, 0.75f) == tc.GetNeighbors(0.5f));
    BOOST_CHECK(std::make_pair(0.75f, 1.0f) == tc.GetNeighbors(0.75f));
    BOOST_CHECK(std::make_pair(0.75f, 1.0f) == tc.GetNeighbors(0.9f));
    BOOST_CHECK(std::make_pair(0.75f, 1.0f) == tc.GetNeighbors(1.0f));
}
