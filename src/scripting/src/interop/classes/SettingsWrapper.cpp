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

bool SettingsWrapper::get_unsh_mask_adaptive() const
{
    return m_Settings.unsharpMasking.adaptive;
}

void SettingsWrapper::unsh_mask_adaptive(bool enabled)
{
    m_Settings.unsharpMasking.adaptive = enabled;
}

double SettingsWrapper::get_unsh_mask_sigma() const
{
    return m_Settings.unsharpMasking.sigma;
}

void SettingsWrapper::unsh_mask_sigma(double value)
{
    if (value <= 0.0)
    {
        throw ScriptExecutionError{std::string{"invalid unsharp mask sigma value: "} + blc(value)};
    }
    m_Settings.unsharpMasking.sigma = value;
}

double SettingsWrapper::get_unsh_mask_amount_min() const
{
    return m_Settings.unsharpMasking.amountMin;
}

void SettingsWrapper::unsh_mask_amount_min(double value)
{
    if (value <= 0.0)
    {
        throw ScriptExecutionError{std::string{"invalid unsharp mask amount min value: "} + blc(value)};
    }
    m_Settings.unsharpMasking.amountMin = value;
}

double SettingsWrapper::get_unsh_mask_amount_max() const
{
    return m_Settings.unsharpMasking.amountMax;
}

void SettingsWrapper::unsh_mask_amount_max(double value)
{
    if (value <= 0.0)
    {
        throw ScriptExecutionError{std::string{"invalid unsharp mask amount max value: "} + blc(value)};
    }
    m_Settings.unsharpMasking.amountMax = value;
}

double SettingsWrapper::get_unsh_mask_amount() const
{
    return m_Settings.unsharpMasking.amountMax;
}

void SettingsWrapper::unsh_mask_amount(double value)
{
    if (value <= 0.0)
    {
        throw ScriptExecutionError{std::string{"invalid unsharp mask amount value: "} + blc(value)};
    }
    m_Settings.unsharpMasking.amountMax = value;
}

double SettingsWrapper::get_unsh_mask_threshold() const
{
    return m_Settings.unsharpMasking.threshold;
}

void SettingsWrapper::unsh_mask_threshold(double value)
{
    if (value < 0.0 || value > 1.0)
    {
        throw ScriptExecutionError{std::string{"invalid unsharp mask threshold value: "} + blc(value)};
    }
    m_Settings.unsharpMasking.threshold = value;
}

double SettingsWrapper::get_unsh_mask_twidth() const
{
    return m_Settings.unsharpMasking.width;
}

void SettingsWrapper::unsh_mask_twidth(double value)
{
    if (value <= 0.0 || value >= 1.0)
    {
        throw ScriptExecutionError{std::string{"invalid unsharp mask transition width value: "} + blc(value)};
    }
    m_Settings.unsharpMasking.width = value;
}

}
