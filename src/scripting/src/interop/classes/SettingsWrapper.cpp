/*
ImPPG (Image Post-Processor) - common operations for astronomical stacks and other images
Copyright (C) 2023-2025 Filip Szczerek <ga.software@yahoo.com>

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
    Settings wrapper implementation.
*/

#include "interop/classes/SettingsWrapper.h"
#include "scripting/script_exceptions.h"

#include <boost/lexical_cast.hpp>

namespace scripting
{

// private definitions
namespace
{

template<typename T>
std::string blc(const T& t) { return boost::lexical_cast<std::string>(t); }

}

SettingsWrapper SettingsWrapper::FromPath(const std::filesystem::path& path)
{
    return SettingsWrapper(path);
}

SettingsWrapper::SettingsWrapper(const std::filesystem::path& path)
{
    const auto settings = LoadSettings(path.native());
    if (!settings.has_value())
    {
        throw ScriptExecutionError(wxString::Format("failed to load settings from %s", path.generic_string()).ToStdString());
    }
    m_Settings = *settings;
}

const ProcessingSettings& SettingsWrapper::GetSettings() const
{
    return m_Settings;
}

bool SettingsWrapper::get_normalization_enabled() const
{
    return m_Settings.normalization.enabled;
}

void SettingsWrapper::normalization_enabled(bool enabled)
{
    m_Settings.normalization.enabled = enabled;
}

double SettingsWrapper::get_normalization_min() const
{
    return m_Settings.normalization.min;
}

void SettingsWrapper::normalization_min(double value)
{
    if (value < 0.0 || value > 1.0)
    {
        throw ScriptExecutionError{std::string{"invalid normalization min value: "} + blc(value)};
    }
    m_Settings.normalization.min = value;
}

double SettingsWrapper::get_normalization_max() const
{
    return m_Settings.normalization.max;
}

void SettingsWrapper::normalization_max(double value)
{
    if (value < 0.0 || value > 1.0)
    {
        throw ScriptExecutionError{std::string{"invalid normalization max value: "} + blc(value)};
    }
    m_Settings.normalization.max = value;
}

double SettingsWrapper::get_lr_deconv_sigma() const
{
    return m_Settings.LucyRichardson.sigma;
}

void SettingsWrapper::lr_deconv_sigma(double value)
{
    if (value <= 0.0)
    {
        throw ScriptExecutionError{std::string{"invalid L-R deconvolution sigma value: "} + blc(value)};
    }
    m_Settings.LucyRichardson.sigma = value;
}

int SettingsWrapper::get_lr_deconv_num_iters() const
{
    return m_Settings.LucyRichardson.iterations;
}

void SettingsWrapper::lr_deconv_num_iters(int value)
{
    if (value < 0)
    {
        throw ScriptExecutionError{std::string{"invalid L-R deconvolution num iters value: "} + blc(value)};
    }
    m_Settings.LucyRichardson.iterations = value;
}

bool SettingsWrapper::get_lr_deconv_deringing() const
{
    return m_Settings.LucyRichardson.deringing.enabled;
}

void SettingsWrapper::lr_deconv_deringing(bool enabled)
{
    m_Settings.LucyRichardson.deringing.enabled = enabled;
}

bool SettingsWrapper::get_unsh_mask_adaptive(int index) const
{
    if (index < 0 || static_cast<std::size_t>(index) >= m_Settings.unsharpMask.size())
    {
        throw ScriptExecutionError{std::string{"invalid unsharp mask index: "} + blc(index)};
    }
    return m_Settings.unsharpMask.at(static_cast<std::size_t>(index)).adaptive;
}

void SettingsWrapper::unsh_mask_adaptive(int index, bool enabled)
{
    if (index < 0 || static_cast<std::size_t>(index) >= m_Settings.unsharpMask.size())
    {
        throw ScriptExecutionError{std::string{"invalid unsharp mask index: "} + blc(index)};
    }
    m_Settings.unsharpMask.at(static_cast<std::size_t>(index)).adaptive = enabled;
}

double SettingsWrapper::get_unsh_mask_sigma(int index) const
{
    if (index < 0 || static_cast<std::size_t>(index) >= m_Settings.unsharpMask.size())
    {
        throw ScriptExecutionError{std::string{"invalid unsharp mask index: "} + blc(index)};
    }
    return m_Settings.unsharpMask.at(static_cast<std::size_t>(index)).sigma;
}

void SettingsWrapper::unsh_mask_sigma(int index, double value)
{
    if (index < 0 || static_cast<std::size_t>(index) >= m_Settings.unsharpMask.size())
    {
        throw ScriptExecutionError{std::string{"invalid unsharp mask index: "} + blc(index)};
    }
    if (value <= 0.0)
    {
        throw ScriptExecutionError{std::string{"invalid unsharp mask sigma value: "} + blc(value)};
    }
    m_Settings.unsharpMask.at(static_cast<std::size_t>(index)).sigma = value;
}

double SettingsWrapper::get_unsh_mask_amount_min(int index) const
{
    if (index < 0 || static_cast<std::size_t>(index) >= m_Settings.unsharpMask.size())
    {
        throw ScriptExecutionError{std::string{"invalid unsharp mask index: "} + blc(index)};
    }
    return m_Settings.unsharpMask.at(static_cast<std::size_t>(index)).amountMin;
}

void SettingsWrapper::unsh_mask_amount_min(int index, double value)
{
    if (index < 0 || static_cast<std::size_t>(index) >= m_Settings.unsharpMask.size())
    {
        throw ScriptExecutionError{std::string{"invalid unsharp mask index: "} + blc(index)};
    }
    if (value <= 0.0)
    {
        throw ScriptExecutionError{std::string{"invalid unsharp mask amount min value: "} + blc(value)};
    }
    m_Settings.unsharpMask.at(static_cast<std::size_t>(index)).amountMin = value;
}

double SettingsWrapper::get_unsh_mask_amount_max(int index) const
{
    if (index < 0 || static_cast<std::size_t>(index) >= m_Settings.unsharpMask.size())
    {
        throw ScriptExecutionError{std::string{"invalid unsharp mask index: "} + blc(index)};
    }
    return m_Settings.unsharpMask.at(static_cast<std::size_t>(index)).amountMax;
}

void SettingsWrapper::unsh_mask_amount_max(int index, double value)
{
    if (index < 0 || static_cast<std::size_t>(index) >= m_Settings.unsharpMask.size())
    {
        throw ScriptExecutionError{std::string{"invalid unsharp mask index: "} + blc(index)};
    }
    if (value <= 0.0)
    {
        throw ScriptExecutionError{std::string{"invalid unsharp mask amount max value: "} + blc(value)};
    }
    m_Settings.unsharpMask.at(static_cast<std::size_t>(index)).amountMax = value;
}

double SettingsWrapper::get_unsh_mask_amount(int index) const
{
    if (index < 0 || static_cast<std::size_t>(index) >= m_Settings.unsharpMask.size())
    {
        throw ScriptExecutionError{std::string{"invalid unsharp mask index: "} + blc(index)};
    }
    return m_Settings.unsharpMask.at(static_cast<std::size_t>(index)).amountMax;
}

void SettingsWrapper::unsh_mask_amount(int index, double value)
{
    if (index < 0 || static_cast<std::size_t>(index) >= m_Settings.unsharpMask.size())
    {
        throw ScriptExecutionError{std::string{"invalid unsharp mask index: "} + blc(index)};
    }
    if (value <= 0.0)
    {
        throw ScriptExecutionError{std::string{"invalid unsharp mask amount value: "} + blc(value)};
    }
    m_Settings.unsharpMask.at(static_cast<std::size_t>(index)).amountMax = value;
}

double SettingsWrapper::get_unsh_mask_threshold(int index) const
{
    if (index < 0 || static_cast<std::size_t>(index) >= m_Settings.unsharpMask.size())
    {
        throw ScriptExecutionError{std::string{"invalid unsharp mask index: "} + blc(index)};
    }
    return m_Settings.unsharpMask.at(static_cast<std::size_t>(index)).threshold;
}

void SettingsWrapper::unsh_mask_threshold(int index, double value)
{
    if (index < 0 || static_cast<std::size_t>(index) >= m_Settings.unsharpMask.size())
    {
        throw ScriptExecutionError{std::string{"invalid unsharp mask index: "} + blc(index)};
    }

    if (value < 0.0 || value > 1.0)
    {
        throw ScriptExecutionError{std::string{"invalid unsharp mask threshold value: "} + blc(value)};
    }
    m_Settings.unsharpMask.at(static_cast<std::size_t>(index)).threshold = value;
}

double SettingsWrapper::get_unsh_mask_twidth(int index) const
{
    if (index < 0 || static_cast<std::size_t>(index) >= m_Settings.unsharpMask.size())
    {
        throw ScriptExecutionError{std::string{"invalid unsharp mask index: "} + blc(index)};
    }
    return m_Settings.unsharpMask.at(static_cast<std::size_t>(index)).width;
}

void SettingsWrapper::unsh_mask_twidth(int index, double value)
{
    if (index < 0 || static_cast<std::size_t>(index) >= m_Settings.unsharpMask.size())
    {
        throw ScriptExecutionError{std::string{"invalid unsharp mask index: "} + blc(index)};
    }
    if (value <= 0.0 || value >= 1.0)
    {
        throw ScriptExecutionError{std::string{"invalid unsharp mask transition width value: "} + blc(value)};
    }
    m_Settings.unsharpMask.at(static_cast<std::size_t>(index)).width = value;
}

int SettingsWrapper::tc_add_point(double x, double y)
{
    if (m_Settings.toneCurve.GetPoint(m_Settings.toneCurve.GetIdxOfClosestPoint(x, y)).x == x)
    {
        throw ScriptExecutionError(std::string{"tone curve point at X = "} + blc(x) + " already exists");
    }

    return static_cast<int>(m_Settings.toneCurve.AddPoint(x, y));
}

void SettingsWrapper::tc_set_point(int index, double x, double y)
{
    auto& tc = m_Settings.toneCurve;
    if (index < 0 || static_cast<std::size_t>(index) >= tc.GetNumPoints())
    {
        throw ScriptExecutionError("tone curve point index out of range");
    }
    if (static_cast<std::size_t>(index) > 0 && static_cast<float>(x) <= tc.GetPoint(index - 1).x)
    {
        throw ScriptExecutionError("cannot set tone curve point at or to the left of its predecessor");
    }
    if (static_cast<std::size_t>(index) < tc.GetNumPoints() - 1 && static_cast<float>(x) >= tc.GetPoint(index + 1).x)
    {
        throw ScriptExecutionError("cannot set tone curve point at or to the right of its successor");
    }

    tc.UpdatePoint(index, x, y);
}

}
