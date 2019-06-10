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

#include <vector>
#include "common.h"

/// Represents a tone curve and associated data. NOTE: LUT contents are not copied by copy constructor and assignment operator.
class c_ToneCurve
{
    /// Look-up table with pre-calculated values of the curve
    float* m_LUT{nullptr};
    /// Number of elements in 'm_LUT'
    int m_LutSize;

    typedef std::vector<FloatPoint_t> FloatPointsVector_t;

    /// Collection of curve points(X = curve argument, Y = curve value), sorted by X
    FloatPointsVector_t m_Points;

    /// Coefficients of a spline. Spline value = a*t^3+b*t^2+c*t+d, where 0<=t<=1
    typedef struct
    {
        float a, b, c, d;
    } SplineParams_t;

    /// Spline coefficients (Catmull-Rom); i-th element corresponds to the interval [m_Points[i]; m_Points[i+1]]
    /** Number of elements = m_Points.size()-1. */
    std::vector<SplineParams_t> m_Spline;

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

    /// Does not copy and does not recalculate LUT contents.
    c_ToneCurve& operator=(const c_ToneCurve& c);

    /// Calculates the look-up table for a quick approximated application of the curve
    void RefreshLut();

    /// Calculates spline coefficients
    void CalculateSpline();

    /// Removes ALL points; before any further operations, at least two points HAVE TO BE added by AddPoint()
    void ClearPoints();

    /// Adds a curve point (keeps all points sorted by their 'x'); returns its index
    int AddPoint(float x, float y);

    /// Removes the specified point. If there are only 2 points, does nothing.
    void RemovePoint(int index);

    bool GetSmooth() const { return m_Smooth; }
    void SetSmooth(bool smooth);

    /// Applies an approximated curve value (one of the pre-calculated values) to 'input'
    /** LUT is not calculated automatically. Caller must call RefreshLut()
        after any update to the curve before using this method. */
    float GetApproximatedValue(
        float input ///< Value from [0.0f; 1.0f]
    ) const
    {
        //TODO: perform a quicker shift when LUT size is a power of 2
        return m_LUT[static_cast<int>(input * (m_LutSize - 1))];
    }

    /// Applies the tone curve to 'input' using a precise curve value
    float GetPreciseValue(
        float input ///< Value from [0.0f; 1.0f]
    ) const;

    /// Returns index of the curve point closest to (x, y)
    int GetIdxOfClosestPoint(
        float x, ///< X coordinate, range [0; 1]
        float y  ///< Y coordinate, range [0; 1]
        ) const;

    const FloatPoint_t& GetPoint(int idx) const
    {
        return m_Points[idx];
    }

    void UpdatePoint(int idx, float x, float y);

    int GetNumPoints() const
    {
        return m_Points.size();
    }

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

    ~c_ToneCurve();
};

#endif
