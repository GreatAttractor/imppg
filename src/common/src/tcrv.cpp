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
    Tone curve class implementation.
*/

#include "common/imppg_assert.h"
#include "common/tcrv.h"
#include "common/common.h"
#include "math_utils/math_utils.h"

#include <algorithm>
#include <limits>
#include <cmath>

const std::size_t DEFAULT_LUT_SIZE = 1 << 16;

c_ToneCurve::c_ToneCurve()
: m_Smooth(true), m_IsGamma(false), m_Gamma(1.0f)
{
    Reset();
}

/// Allocates a LUT, but does not copy and does not recalculate LUT contents.
c_ToneCurve::c_ToneCurve(const c_ToneCurve& c)
: m_Points(c.m_Points),
  m_Spline(c.m_Spline),
  m_Smooth(c.m_Smooth),
  m_IsGamma(c.m_IsGamma),
  m_Gamma(c.m_Gamma)
{}

c_ToneCurve::c_ToneCurve(std::initializer_list<FloatPoint_t> points)
: m_Smooth(true), m_IsGamma(false), m_Gamma(1.0f)
{
    for (const auto& point: points)
    {
        AddPoint(point.x, point.y);
    }
    CalculateSpline();
    RefreshLut();
}

/// Does not copy and does not recalculate LUT contents.
c_ToneCurve& c_ToneCurve::operator=(const c_ToneCurve& c)
{
    m_Points = c.m_Points;
    m_Spline = c.m_Spline;
    m_Smooth = c.m_Smooth;
    m_Gamma = c.m_Gamma;
    m_IsGamma = c.m_IsGamma;

    m_LUT = std::nullopt;

    return *this;
}

void c_ToneCurve::UpdatePoint(std::size_t idx, float x, float y)
{
    IMPPG_ASSERT(idx < m_Points.size());
    if (idx > 0) { IMPPG_ASSERT(m_Points[idx - 1].x < x); }
    if (static_cast<std::size_t>(idx) < m_Points.size() - 1) { IMPPG_ASSERT(x < m_Points[idx + 1].x); }

    m_Points[idx].x = x;
    m_Points[idx].y = y;
    if (m_Smooth)
        CalculateSpline();
}

/// Removes ALL points; before any further operations, at least two points HAVE TO BE added by AddPoint()
void c_ToneCurve::ClearPoints()
{
    m_Points.clear();
    m_Spline.clear();
}

/// Calculates spline coefficients
void c_ToneCurve::CalculateSpline()
{
    m_Spline.clear();

    for (unsigned i = 0; i < m_Points.size() - 1; i++)
    {
        float dXstart, dXend,
            dYstart, dYend;

        if (i == 0)
        {
            dXstart = m_Points[i+1].x - m_Points[i].x;
            dYstart = m_Points[i+1].y - m_Points[i].y;
        }
        else
        {
            dXstart = m_Points[i+1].x - m_Points[i-1].x;
            dYstart = m_Points[i+1].y - m_Points[i-1].y;
        }

        if (i == m_Points.size() - 2)
        {
            dXend = m_Points[i+1].x - m_Points[i].x;
            dYend = m_Points[i+1].y - m_Points[i].y;
        }
        else
        {
            dXend = m_Points[i+2].x - m_Points[i].x;
            dYend = m_Points[i+2].y - m_Points[i].y;
        }

        float dx = m_Points[i+1].x - m_Points[i].x;

        // Left-side and right-side tangents of the i-th interval
        float tan1 = dYstart/dXstart,
              tan2 = dYend / dXend;

        // Tangents have to be scaled by 'dx', as later we will use an argument 0<=t<=1
        tan1 *= dx;
        tan2 *= dx;

        SplineParams sp;

        // Treat the first and last intervals specially and use 2-degree (quadratic) curves to avoid inflection points
        if (m_Points.size() > 2 && i == 0)
        {
            FloatPoint_t &p0 = m_Points[0], &p1 = m_Points[1], &p2 = m_Points[2];

            // Tangent on the right end of the current interval
            float tanr = (p2.y - p0.y)/(p2.x - p0.x) * (p1.x - p0.x);

            sp.a = 0;
            sp.d = p0.y;
            sp.b = tanr - p1.y + sp.d;
            sp.c = p1.y - sp.b - sp.d;
        }
        else if (m_Points.size() > 2 && i == m_Points.size() - 2)
        {
            FloatPoint_t &p0 = m_Points[m_Points.size()-1],
                         &p1 = m_Points[m_Points.size()-2],
                         &p2 = m_Points[m_Points.size()-3];

            // Tangent on the left end of the current interval
            float tanl = (p0.y - p2.y)/(p0.x - p2.x) * (p0.x - p1.x);

            sp.a = 0;
            sp.d = p1.y;
            sp.c = tanl;
            sp.b = p0.y - sp.c - sp.d;
        }
        else // Use a cubic spline
        {
            sp.d = m_Points[i].y;
            sp.c = tan1;
            sp.a = tan2 - 2*m_Points[i+1].y + sp.c + 2*sp.d;
            sp.b = m_Points[i+1].y - sp.a - sp.c - sp.d;
        }

        m_Spline.push_back(sp);
    }
}

void c_ToneCurve::SetSmooth(bool smooth)
{
    if (!m_Smooth && smooth)
        CalculateSpline();

    m_Smooth = smooth;
}

/// Resets the curve to identity map (linear from (0,0) to (1,1))
void c_ToneCurve::Reset()
{
    m_Points.clear();
    m_Points.push_back(FloatPoint_t(0.0f, 0.0f));
    m_Points.push_back(FloatPoint_t(1.0f, 1.0f));
    m_IsGamma = false;
    m_Gamma = 1.0f;
    CalculateSpline();
    m_Smooth = true;
}

std::size_t c_ToneCurve::GetIdxOfClosestPoint(float x, float y) const
{
    float minDistSq = 1.0e+6f;
    auto minIdx = std::numeric_limits<std::size_t>::max();

    // The number of points will not be high, so just check all of them
    for (std::size_t i = 0; i < m_Points.size(); i++)
    {
        float distSq = sqr(m_Points[i].x - x) + sqr(m_Points[i].y - y);
        if (distSq < minDistSq)
        {
            minDistSq = distSq;
            minIdx = i;
        }
    }

    return minIdx;
}

/// Calculates the 16-bit Look-Up Table for a quick approximated application of the curve
void c_ToneCurve::RefreshLut()
{
    m_LUT = std::vector<float>{};
    for (size_t i = 0; i < DEFAULT_LUT_SIZE; i++)
        m_LUT->push_back(GetPreciseValue(i * 1.0f/(DEFAULT_LUT_SIZE - 1)));
}

/// Applies the tone curve to 'input' using a precise curve value
float c_ToneCurve::GetPreciseValue(
    float input ///< Value from [0.0f; 1.0f]
) const
{
    float result;

    if (m_IsGamma)
    {
        // Return a gamma curve with ends at the first and last curve points

        if (input <= m_Points[0].x)
            return m_Points[0].y;
        else if (input >= m_Points[1].x)
            return m_Points[1].y;
        else
            result = m_Points[0].y + powf((input - m_Points[0].x) / (m_Points[1].x - m_Points[0].x), 1/m_Gamma)
                * (m_Points[1].y - m_Points[0].y);
    }
    else
    {
        // Index of the point with X >= input
        std::size_t nextIdx = std::lower_bound(m_Points.cbegin(), m_Points.cend(), FloatPoint_t(input, 0)) - m_Points.begin();

        // If 'input' is at or past the last point, return the value at the last point
        if (nextIdx == m_Points.size() && input >= m_Points.back().x)
            return m_Points.back().y;

        // If 'input' is at or before the first point, return the value at the first point
        if (nextIdx == 0)
            return m_Points[0].y;

        //TODO: precalculate the deltas
        float deltaX = m_Points[nextIdx].x - m_Points[nextIdx-1].x;

        if (!m_Smooth) // The curve is piecewise linear
        {
            float deltaY = m_Points[nextIdx].y - m_Points[nextIdx-1].y;
            result = m_Points[nextIdx-1].y + deltaY * (input - m_Points[nextIdx-1].x) / deltaX;
        }
        else // The curve consists of Catmull-Rom splines
        {
            float t = (input - m_Points[nextIdx-1].x) / deltaX;

            const SplineParams& sp = m_Spline[nextIdx-1];
            result = t*(t*(t * sp.a + sp.b) + sp.c) + sp.d;
        }
    }

    if (result < 0.0f)
        result = 0.0f;
    else if (result > 1.0f)
        result = 1.0f;
    return result;
}

/// Removes the specified point. If there are only 2 points, does nothing.
void c_ToneCurve::RemovePoint(std::size_t index)
{
    IMPPG_ASSERT(index < m_Points.size());
    if (m_Points.size() > 2)
    {
        m_Points.erase(m_Points.begin() + index);
        CalculateSpline();
    }
}

/// Adds a curve point; returns its index
std::size_t c_ToneCurve::AddPoint(float x, float y)
{
    const auto insertAt = std::lower_bound(m_Points.begin(), m_Points.end(), FloatPoint_t(x, 0.0f));
    const std::size_t result = insertAt - m_Points.begin();
    m_Points.insert(insertAt, FloatPoint_t(x, y));

    if (result > 0) { IMPPG_ASSERT(m_Points[result - 1].x < x); }
    if (result < m_Points.size() - 1) { IMPPG_ASSERT(m_Points[result].x < m_Points[result + 1].x); }

    if (m_Smooth)
    {
        CalculateSpline();
    }

    return result;
}

/// If 'isGammaMode', the curve is defined as output = input^(1/m_Gamma)
void c_ToneCurve::SetGammaMode(bool isGammaMode)
{
    m_IsGamma = isGammaMode;
    if (isGammaMode && m_Points.size() > 2)
    {
        FloatPoint_t pFirst = m_Points.front(),
                     pLast = m_Points.back();

        m_Points.clear();
        m_Points.push_back(pFirst);
        m_Points.push_back(pLast);
        CalculateSpline();
    }
    else
        CalculateSpline();
}

/// Inverts the curve to create a negative image (reflects all points horizontally)
void c_ToneCurve::Invert()
{
    float xmax = m_Points.front().x,
          xmin = m_Points.back().x;

    std::vector<FloatPoint_t> newPoints;
    for (int i = static_cast<int>(m_Points.size())-1; i >= 0; i--)
        newPoints.push_back(FloatPoint_t(xmin + xmax - m_Points[i].x, m_Points[i].y));

    m_Points = newPoints;
    CalculateSpline();
}

/// Stretches the points to fill the interval [min; max]
void c_ToneCurve::Stretch(float min, float max)
{
    float currentMin = m_Points.front().x,
          currentMax = m_Points.back().x;

    std::vector<FloatPoint_t> newPoints;
    for (unsigned i = 0; i < m_Points.size(); i++)
        newPoints.push_back(FloatPoint_t(min + (m_Points[i].x - currentMin) * (max - min) / (currentMax - currentMin),
                m_Points[i].y));

    m_Points = newPoints;
    CalculateSpline();
}

bool c_ToneCurve::IsIdentity() const
{
    const bool isFrom0To1 =
        m_Points.size() == 2 &&
        m_Points[0].x == 0.0f &&
        m_Points[0].y == 0.0f &&
        m_Points[1].x == 1.0f &&
        m_Points[1].y == 1.0f;

    return isFrom0To1 && (!m_IsGamma || m_Gamma == 1.0f);
}

std::pair<float, float> c_ToneCurve::GetNeighbors(float x)
{
    const auto numPoints = m_Points.size();
    IMPPG_ASSERT(numPoints >= 2);

    if (0.0f == x)
    {
        if (m_Points.at(0).x == 0) { return {0.0f, m_Points.at(1).x}; }
        else { return {0.0f, m_Points.at(0).x}; }
    }

    if (1.0f == x)
    {
        if (m_Points.at(numPoints - 1).x == 1.0f) { return {m_Points.at(numPoints - 2).x, 1.0f}; }
        else { return {m_Points.at(numPoints - 1).x, 1.0f}; }
    }

    if (x < m_Points.at(0).x)
    {
        return {0.0f, m_Points.at(0).x};
    }

    if (x >= m_Points.at(numPoints - 1).x)
    {
        return {m_Points.at(numPoints - 1).x, 1.0f};
    }

    for (std::size_t i = 0; i <= numPoints - 2; ++i)
    {
        const float left = m_Points.at(i).x;
        const float right = m_Points.at(i + 1).x;
        if (left <= x && x < right)
        {
            return {left, right};
        }
    }

    IMPPG_ABORT_MSG("failed to determine neighbors");
}

bool c_ToneCurve::HasPointAt(float x) const
{
    for (const auto& point: m_Points)
    {
        if (point.x == x) { return true; }
    }

    return false;
}
