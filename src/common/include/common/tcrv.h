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
    Tone curve class header.
*/

#ifndef IMPPG_TONE_CURVE_H
#define IMPPG_TONE_CURVE_H

#include <initializer_list>
#include <optional>
#include <utility>
#include <vector>

#include "common/common.h"
#include "common/imppg_assert.h"

/// Represents a tone curve and associated data. NOTE: LUT contents are not copied by copy constructor and assignment operator.
class c_ToneCurve
{
public:
    /// Coefficients of a spline. Spline value = a*t^3+b*t^2+c*t+d, where 0<=t<=1.
    struct SplineParams
    {
        float a, b, c, d;
    };

private:
    /// Look-up table with pre-calculated values of the curve.
    std::optional<std::vector<float>> m_LUT;

    /// Collection of curve points(X = curve argument, Y = curve value), sorted by X
    std::vector<FloatPoint_t> m_Points;

    /// Spline coefficients (Catmull-Rom); i-th element corresponds to the interval [m_Points[i]; m_Points[i+1]]
    /** Number of elements = m_Points.size()-1. */
    std::vector<SplineParams> m_Spline;

    /// If 'true', control points are interpolated by a Catmull-Rom spline
    bool m_Smooth;

    /// 'True' if the curve is defined as output = input^(1/m_Gamma)
    bool m_IsGamma;

    /// The curve is defined as output = input^(1/m_Gamma) if 'm_IsGamma' equals 'true'
    float m_Gamma;

public:
    /// Default constructor: sets the curve to identity map (linear from (0,0) to (1,1)) and calculates the LUT
    c_ToneCurve();

    /// Allocates a LUT, but does not copy and does not recalculate LUT contents.
    c_ToneCurve(const c_ToneCurve& c);

    c_ToneCurve(std::initializer_list<FloatPoint_t> points);

    /// Does not copy and does not recalculate LUT contents.
    c_ToneCurve& operator=(const c_ToneCurve& c);

    /// Calculates the look-up table for a quick approximated application of the curve
    void RefreshLut();

    /// Calculates spline coefficients
    void CalculateSpline();

    /// Removes ALL points; before any further operations, at least two points HAVE TO BE added by AddPoint()
    void ClearPoints();

    /// Adds a curve point (keeps all points sorted by their 'x'); returns its index
    std::size_t AddPoint(float x, float y);

    bool HasPointAt(float x) const;

    /// Removes the specified point. If there are only 2 points, does nothing.
    void RemovePoint(std::size_t index);

    bool GetSmooth() const { return m_Smooth; }
    void SetSmooth(bool smooth);

    /// Tone-maps `input` to `output` using approximated tone curve values.
    /** LUT is not calculated automatically. Caller must call RefreshLut() after any update to the curve before using this method. */
    void ApplyApproximatedToneCurve(const float input[], float output[], size_t length)
    {
        IMPPG_ASSERT(m_LUT.has_value());
        for (size_t i = 0; i < length; i++)
            output[i] = (*m_LUT)[static_cast<int>(input[i] * (m_LUT->size() - 1))];
    }

    /// Tone-maps `input` to `output` using precise tone curve values.
    void ApplyPreciseToneCurve(const float input[], float output[], size_t length) const
    {
        for (size_t i = 0; i < length; i++)
            output[i] = GetPreciseValue(input[i]);
    }

    /// Applies the tone curve to 'input' using a precise curve value
    float GetPreciseValue(
        float input ///< Value from [0.0f; 1.0f]
    ) const;

    /// Returns index of the curve point closest to (x, y)
    std::size_t GetIdxOfClosestPoint(
        float x, ///< X coordinate, range [0; 1]
        float y  ///< Y coordinate, range [0; 1]
    ) const;

    const FloatPoint_t& GetPoint(std::size_t idx) const
    {
        return m_Points[idx];
    }

    void UpdatePoint(std::size_t idx, float x, float y);

    std::size_t GetNumPoints() const
    {
        return m_Points.size();
    }

    const std::vector<FloatPoint_t>& GetPoints() const { return m_Points; }

    const std::vector<SplineParams>& GetSplines() const { return m_Spline; }

    /// Returns 'true' if the curve is defined as output = input^(1/m_Gamma)
    bool IsGammaMode() const { return m_IsGamma; }

    /// If 'isGammaMode', the curve is defined as output = input^(1/m_Gamma)
    void SetGammaMode(bool isGammaMode);

    float GetGamma() const { return m_Gamma; }

    void SetGamma(float gamma) { m_Gamma = gamma; }

    /// Resets the curve to identity map (linear from (0,0) to (1,1))
    void Reset();

    /// Inverts the curve to create a negative image (reflects all points horizontally)
    void Invert();

    /// Stretches the points to fill the interval [min; max]
    void Stretch(float min, float max);

    /// Returns `true` if the tone curve is an identity map (no impact on the image).
    bool IsIdentity() const;

    bool operator==(const c_ToneCurve& other) const
    {
        return m_Points == other.m_Points
            && m_Smooth == other.m_Smooth
            && m_IsGamma == other.m_IsGamma
            && m_Gamma == other.m_Gamma;
    }

    /// Returns X coordinates of the closest curve points to the left and right of `x`.
    std::pair<float, float> GetNeighbors(float x);
};

#endif
