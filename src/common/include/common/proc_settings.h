/*
ImPPG (Image Post-Processor) - common operations for astronomical stacks and other images
Copyright (C) 2016-2025 Filip Szczerek <ga.software@yahoo.com>

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
    Processing settings structure.
*/

#ifndef IMPGG_PROCESSING_SETTINGS_HEADER
#define IMPGG_PROCESSING_SETTINGS_HEADER

#include "common/tcrv.h"

#include <array>
#include <string>
#include <vector>
#include <wx/stream.h>
#include <wx/string.h>

namespace Default
{
    constexpr float UNSHMASK_SIGMA = 1.3f;
    constexpr float UNSHMASK_AMOUNT = 1.0f;
    constexpr float UNSHMASK_THRESHOLD = 0.01f;
    constexpr float UNSHMASK_WIDTH = 0.1f;
}

/// Default-constructed value does not have any effect on image.
struct UnsharpMask
{
    bool adaptive{false}; ///If true, adaptive unsharp masking is used.
    float sigma{Default::UNSHMASK_SIGMA}; ///< Gaussian kernel sigma.
    float amountMin{Default::UNSHMASK_AMOUNT}; ///< Amount (weight) of the unsharped layer; <1.0 blurs, >1.0 sharpens; if adaptive=true, used as the min amount.
    float amountMax{Default::UNSHMASK_AMOUNT}; ///< Max amount.
    float threshold{Default::UNSHMASK_THRESHOLD}; ///< Threshold of input image brightness where the min-max amount transition occurs.
    float width{Default::UNSHMASK_WIDTH}; ///< Width of the transition interval.

    bool IsEffective() const
    {
        return !adaptive && amountMax != 1.0f ||
            adaptive && (amountMin != 1.0f || amountMax != 1.0f);
    }

    bool operator==(const UnsharpMask& other) const
    {
        return adaptive == other.adaptive
            && sigma == other.sigma
            && amountMin == other.amountMin
            && amountMax == other.amountMax
            && threshold == other.threshold
            && width == other.width;
    }
};

/// Default-constructed settings do not have any effect on image.
struct ProcessingSettings
{
    /// Normalization is performed prior to all other processing steps.
    struct
    {
        bool enabled{false};
        float min{0.0}, max{1.0};
    } normalization;

    struct
    {
        float sigma{1.0}; ///< Lucy-Richardson deconvolution kernel sigma
        int iterations{0}; ///< Number of Lucy-Richardson deconvolution iterations.
        struct
        {
            bool enabled{false}; ///< Experimantal; enables deringing along edges of overexposed areas (see c_LucyRichardsonThread::DoWork()).
        } deringing;
    } LucyRichardson;

    // By convention, there is always at least one element (may be a no-op, i.e. amount = 1.0).
    std::vector<UnsharpMask> unsharpMask{UnsharpMask{}};

    c_ToneCurve toneCurve;

    bool operator==(const ProcessingSettings& other) const
    {
        return normalization.enabled == other.normalization.enabled
            && normalization.min == other.normalization.min
            && normalization.max == other.normalization.max
            && LucyRichardson.sigma == other.LucyRichardson.sigma
            && LucyRichardson.iterations == other.LucyRichardson.iterations
            && LucyRichardson.deringing.enabled == other.LucyRichardson.deringing.enabled
            && unsharpMask == other.unsharpMask
            && toneCurve == other.toneCurve;
    }

    bool AdaptiveUnshMaskEnabled() const
    {
        for (const auto& umask: unsharpMask)
        {
            if (umask.adaptive) { return true; }
        }
        return false;
    }
};

void SerializeSettings(const ProcessingSettings& settings, wxOutputStream& stream);

std::optional<ProcessingSettings> DeserializeSettings(wxInputStream& stream);

/// Saves the settings of Lucy-Richardson deconvolution, unsharp masking, tone curve and deringing; returns 'false' on error.
bool SaveSettings(const wxString& filePath, const ProcessingSettings& settings);

/// Loads the settings of Lucy-Richardson deconvolution, unsharp masking, tone curve and deringing; returns 'false' on error.
std::optional<ProcessingSettings> LoadSettings(const wxString& filePath);

/// Returns the coefficients a, b, c, d of the cubic curve defining the "amount" value
/// for adaptive unsharp masking. The amount is a function of local brightness
/// (of the raw input image) as follows:
///
///   - if brightness < threshold - width, amount is amountMin
///   - if brightness > threshold + width, amount is amountMax
///   - transition region: if threshold - width <= brightness <= threshold - width,
///     amount changes smoothly from amounMin to amountMax following a cubic function:
///
///     amount = a*brightness^3 + b*brightness^2 + c*brightness + d
///
///  such that its derivatives are zero at (threshold - width) and (threshold + width)
///  and there is an inflection point at the threshold.
///
std::array<float, 4> GetAdaptiveUnshMaskTransitionCurve(const UnsharpMask& um);

#endif // IMPGG_PROCESSING_SETTINGS_HEADER
