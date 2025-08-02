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
    Processing settings loading/saving implementation.
*/

#include "common/num_formatter.h"
#include "common/proc_settings.h"

#include <sstream>
#include <wx/wfstream.h>
#include <wx/xml/xml.h>

// private definitions
namespace
{

const int XML_INDENT = 4;
const unsigned FLOAT_PREC = 5;

/// Names of XML elements in a settings file
namespace XmlName
{
    const char* root = "imppg";

    const char* lucyRichardson = "lucy-richardson";
    const char* lrSigma = "sigma";
    const char* lrIters = "iterations";
    const char* lrDeringing = "deringing";

    const char* unshMaskList = "unsharp_mask_list";
    const char* unshMask = "unsharp_mask";
    const char* unshAdaptive = "adaptive";
    const char* unshSigma = "sigma";
    const char* unshAmountMin = "amount_min";
    const char* unshAmountMax = "amount_max";
    const char* unshThreshold = "amount_threshold";
    const char* unshWidth = "amount_width";

    const char* tcurve = "tone_curve";
    const char* tcSmooth = "smooth";
    const char* tcIsGamma = "is_gamma";
    const char* tcGamma = "gamma";

    const char* normalization = "normalization";
    const char* normEnabled = "enabled";
    const char* normMin = "min";
    const char* normMax = "max";
}

const char* trueStr = "true";
const char* falseStr = "false";

wxXmlNode* CreateToneCurveSettingsNode(const c_ToneCurve& toneCurve)
{
    wxXmlNode *result = new wxXmlNode(wxXML_ELEMENT_NODE, XmlName::tcurve);
    result->AddAttribute(XmlName::tcSmooth, toneCurve.GetSmooth() ? trueStr : falseStr);
    result->AddAttribute(XmlName::tcIsGamma, toneCurve.IsGammaMode() ? trueStr : falseStr);
    if (toneCurve.IsGammaMode())
         result->AddAttribute(XmlName::tcGamma, NumFormatter::Format(toneCurve.GetGamma(), FLOAT_PREC));
    wxString pointsStr;
    for (std::size_t i = 0; i < toneCurve.GetNumPoints(); i++)
    {
        pointsStr += NumFormatter::Format(toneCurve.GetPoint(i).x, FLOAT_PREC) + ";"+
                     NumFormatter::Format(toneCurve.GetPoint(i).y, FLOAT_PREC) + ";";
    }

    result->AddChild(new wxXmlNode(wxXML_TEXT_NODE, wxEmptyString, pointsStr));

    return result;
}

wxXmlNode* CreateLucyRichardsonSettingsNode(float lrSigma, int lrIters, bool lrDeringing)
{
    wxXmlNode* result = new wxXmlNode(wxXML_ELEMENT_NODE, XmlName::lucyRichardson);
    result->AddAttribute(XmlName::lrSigma, NumFormatter::Format(lrSigma, FLOAT_PREC));
    result->AddAttribute(XmlName::lrIters, wxString::Format("%d", lrIters));
    result->AddAttribute(XmlName::lrDeringing, lrDeringing ? trueStr : falseStr);
    return result;
}

wxXmlNode* CreateUnsharpMaskNode(const UnsharpMask& unsharpMask)
{
    wxXmlNode* result = new wxXmlNode(wxXML_ELEMENT_NODE, XmlName::unshMask);

    result->AddAttribute(XmlName::unshAdaptive, unsharpMask.adaptive ? trueStr : falseStr);
    result->AddAttribute(XmlName::unshSigma, NumFormatter::Format(unsharpMask.sigma, FLOAT_PREC));
    result->AddAttribute(XmlName::unshAmountMin, NumFormatter::Format(unsharpMask.amountMin, FLOAT_PREC));
    result->AddAttribute(XmlName::unshAmountMax, NumFormatter::Format(unsharpMask.amountMax, FLOAT_PREC));
    result->AddAttribute(XmlName::unshThreshold, NumFormatter::Format(unsharpMask.threshold, FLOAT_PREC));
    result->AddAttribute(XmlName::unshWidth, NumFormatter::Format(unsharpMask.width, FLOAT_PREC));
    return result;
}

wxXmlNode* CreateUnsharpMaskListNode(const std::vector<UnsharpMask>& masks)
{
    wxXmlNode* result = new wxXmlNode(wxXML_ELEMENT_NODE, XmlName::unshMaskList);

    for (const auto& mask: masks)
    {
        result->AddChild(CreateUnsharpMaskNode(mask));
    }

    return result;
}

wxXmlNode* CreateNormalizationSettingsNode(bool normalizationEnabled, float normMin, float normMax)
{
    wxXmlNode* result = new wxXmlNode(wxXML_ELEMENT_NODE, XmlName::normalization);
    result->AddAttribute(XmlName::normEnabled, normalizationEnabled ? trueStr : falseStr);
    result->AddAttribute(XmlName::normMin, NumFormatter::Format(normMin, FLOAT_PREC));
    result->AddAttribute(XmlName::normMax, NumFormatter::Format(normMax, FLOAT_PREC));
    return result;
}

bool ParseLucyRichardsonSettings(const wxXmlNode* node, float& sigma, int& iterations, bool& deringing)
{
    if (!NumFormatter::Parse(node->GetAttribute(XmlName::lrSigma), sigma))
    {
        return false;
    }

    unsigned long value{};
    if (!node->GetAttribute(XmlName::lrIters).ToULong(&value))
    {
        return false;
    }
    else
    {
        iterations = static_cast<int>(value);
    }

    if (node->GetAttribute(XmlName::lrDeringing) == trueStr)
        deringing = true;
    else if (node->GetAttribute(XmlName::lrDeringing) == falseStr)
        deringing = false;
    else
        return false;

    return true;
}

std::optional<UnsharpMask> ParseUnsharpMaskingSettings(const wxXmlNode* node)
{
    std::stringstream parser;

    UnsharpMask result;

    if (node->GetAttribute(XmlName::unshAdaptive) == trueStr)
    {
        result.adaptive = true;
    }
    else if (node->GetAttribute(XmlName::unshAdaptive) == falseStr)
    {
        result.adaptive = false;
    }
    else
    {
        return std::nullopt;
    }

    if (!NumFormatter::Parse(node->GetAttribute(XmlName::unshSigma), result.sigma))
    {
        return std::nullopt;
    }

    if (!NumFormatter::Parse(node->GetAttribute(XmlName::unshAmountMin), result.amountMin))
    {
        return std::nullopt;
    }

    if (!NumFormatter::Parse(node->GetAttribute(XmlName::unshAmountMax), result.amountMax))
    {
        return std::nullopt;
    }

    if (!NumFormatter::Parse(node->GetAttribute(XmlName::unshThreshold), result.threshold))
    {
        return std::nullopt;
    }

    if (!NumFormatter::Parse(node->GetAttribute(XmlName::unshWidth), result.width))
    {
        return std::nullopt;
    }

    return result;
}

bool ParseNormalizationSetings(const wxXmlNode* node, bool& normalizationEnabled, float& normMin, float& normMax)
{
    if (node->GetAttribute(XmlName::normEnabled) == "true")
        normalizationEnabled = true;
    else if (node->GetAttribute(XmlName::normEnabled) == "false")
        normalizationEnabled = false;
    else
        return false;

    if (!NumFormatter::Parse(node->GetAttribute(XmlName::normMin), normMin))
    {
        return false;
    }

    if (!NumFormatter::Parse(node->GetAttribute(XmlName::normMax), normMax))
    {
        return false;
    }

    return true;
}

bool ParseToneCurveSettings(const wxXmlNode* node, c_ToneCurve& tcurve)
{
    wxString boolStr = node->GetAttribute(XmlName::tcSmooth);
    if (boolStr == "true")
        tcurve.SetSmooth(true);
    else if (boolStr == "false")
        tcurve.SetSmooth(false);
    else
        return false;

    boolStr = node->GetAttribute(XmlName::tcIsGamma);
    if (boolStr == "true")
    {
        tcurve.SetGammaMode(true);

        float gamma{};
        if (!NumFormatter::Parse(node->GetAttribute(XmlName::tcGamma), gamma))
        {
            return false;
        }
        else
        {
            tcurve.SetGamma(gamma);
        }
    }
    else if (boolStr == "false")
        tcurve.SetGammaMode(false);
    else
        return false;

    std::vector<float> points_xy;
    if (!NumFormatter::ParseList(node->GetNodeContent(), points_xy, ';'))
    {
        return false;
    }
    if (points_xy.size() % 2 != 0)
    {
        return false;
    }

    tcurve.ClearPoints();
    std::optional<float> lastX;

    // deals with points with repeated X values (may be present in some settings files saved in older buggy versions);
    // they will be reduced to one point with averaged Y value
    struct
    {
        double x{0.0};
        std::size_t count{0};
        double ySum{0.0};

        void AddPoint(float y)
        {
            count += 1;
            ySum += y;
        }

        float GetY() const
        {
            return static_cast<float>(ySum / count);
        }

        void Reset(float newX)
        {
            x = newX;
            count = 0;
            ySum = 0.0;
        }
    } accumulator;

    for (size_t i = 0; i < points_xy.size(); i += 2)
    {
        const auto x = points_xy[i];
        const auto y = points_xy[i + 1];
        if (x < 0.0 || x > 1.0 || y < 0.0 || y > 1.0 || lastX.has_value() && x < *lastX) { return false; }

        if (lastX.has_value() && x > *lastX)
        {
            tcurve.AddPoint(accumulator.x, accumulator.GetY());
            accumulator.Reset(x);
            accumulator.AddPoint(y);
            lastX = x;
        }
        else if (lastX.has_value() && x == *lastX)
        {
            accumulator.AddPoint(y);
        }
        else if (!lastX.has_value())
        {
            accumulator.Reset(x);
            accumulator.AddPoint(y);
            lastX = x;
        }
    }

    if (accumulator.count > 0)
    {
        tcurve.AddPoint(accumulator.x, accumulator.GetY());
    }

    if (tcurve.GetNumPoints() < 2)
        return false;
    else
        return true;
}

} // end of private definitions

bool SaveSettings(const std::string& filePath, const ProcessingSettings& settings)
{
    wxFileOutputStream file{filePath};
    SerializeSettings(settings, file);
    return file.IsOk();
}

std::optional<ProcessingSettings> LoadSettings(const std::string& filePath)
{
    wxFileInputStream stream{filePath};
    return DeserializeSettings(stream);
}

std::array<float, 4> GetAdaptiveUnshMaskTransitionCurve(const UnsharpMask& um)
{
    const float divisor = 4 * um.width * um.width * um.width;
    const float a = (um.amountMin - um.amountMax) / divisor;
    const float b = 3 * (um.amountMax - um.amountMin) * um.threshold / divisor;
    const float c = 3 * (um.amountMax - um.amountMin) * (um.width - um.threshold) * (um.width + um.threshold) / divisor;
    const float d = (2 * um.width * um.width * um.width * (um.amountMin + um.amountMax) +
        3 * um.threshold * um.width * um.width * (um.amountMin - um.amountMax) +
        um.threshold * um.threshold * um.threshold * (um.amountMax - um.amountMin)) / divisor;

    return {a, b, c, d};
}

void SerializeSettings(const ProcessingSettings& settings, wxOutputStream& stream)
{
    wxXmlNode* root = new wxXmlNode(wxXML_ELEMENT_NODE, XmlName::root);

    root->AddChild(CreateLucyRichardsonSettingsNode(
        settings.LucyRichardson.sigma,
        settings.LucyRichardson.iterations,
        settings.LucyRichardson.deringing.enabled
    ));

    root->AddChild(CreateUnsharpMaskListNode(settings.unsharpMask));

    root->AddChild(CreateToneCurveSettingsNode(settings.toneCurve));

    root->AddChild(CreateNormalizationSettingsNode(
        settings.normalization.enabled,
        settings.normalization.min,
        settings.normalization.max
    ));

    wxXmlDocument xdoc;
    xdoc.SetVersion("1.0");
    xdoc.SetFileEncoding("UTF-8");
    xdoc.SetRoot(root);
    xdoc.Save(stream, XML_INDENT);
}

std::optional<ProcessingSettings> DeserializeSettings(wxInputStream& stream)
{
    ProcessingSettings settings{};

    wxXmlDocument xdoc;
    if (!xdoc.Load(stream)) { return std::nullopt; }

    wxXmlNode* root = xdoc.GetRoot();
    if (!root) { return std::nullopt; }

    wxXmlNode* child = root->GetChildren();
    while (child)
    {
        if (child->GetName() == XmlName::lucyRichardson)
        {
            float sigma;
            int iters;
            bool deringing;

            if (!ParseLucyRichardsonSettings(child, sigma, iters, deringing)) { return std::nullopt; }

            settings.LucyRichardson.sigma = sigma;
            settings.LucyRichardson.iterations = iters;
            settings.LucyRichardson.deringing.enabled = deringing;
        }
        else if (child->GetName() == XmlName::unshMask) // legacy settings with 1 unsharp mask
        {
            const auto unsharpMask = ParseUnsharpMaskingSettings(child);
            if (!unsharpMask.has_value()) { return std::nullopt; }

            settings.unsharpMask = { unsharpMask.value() };
        }
        else if (child->GetName() == XmlName::unshMaskList)
        {
            settings.unsharpMask.clear();

            wxXmlNode* maskNode = child->GetChildren();
            while (maskNode)
            {
                const auto unsharpMask = ParseUnsharpMaskingSettings(maskNode);
                if (!unsharpMask.has_value()) { return std::nullopt; }
                settings.unsharpMask.push_back(unsharpMask.value());
                maskNode = maskNode->GetNext();
            }

            if (settings.unsharpMask.empty()) { return std::nullopt; }
        }
        else if (child->GetName() == XmlName::tcurve)
        {
            c_ToneCurve tcurve;
            if (!ParseToneCurveSettings(child, tcurve)) { return std::nullopt; }

            settings.toneCurve = tcurve;
        }
        else if (child->GetName() == XmlName::normalization)
        {
            bool enabled;
            float nmin, nmax;

            if (!ParseNormalizationSetings(child, enabled, nmin, nmax)) { return std::nullopt; }
            settings.normalization.enabled = enabled;
            settings.normalization.min = nmin;
            settings.normalization.max = nmax;
        }

        child = child->GetNext();
    }

    return settings;
}
