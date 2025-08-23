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
    Settings wrapper header.
*/
#pragma once

#include "common/proc_settings.h"
#include "interop/classes/method.h"

#include <filesystem>
#include <lua.hpp>
#include <memory>
#include <string>

namespace scripting
{

class SettingsWrapper
{
public:
    SettingsWrapper() = default;

    static SettingsWrapper FromPath(const std::filesystem::path& path);

    bool get_normalization_enabled() const;
    void normalization_enabled(bool enabled);

    double get_normalization_min() const;
    void normalization_min(double value);

    double get_normalization_max() const;
    void normalization_max(double value);

    double get_lr_deconv_sigma() const;
    void lr_deconv_sigma(double value);

    int get_lr_deconv_num_iters() const;
    void lr_deconv_num_iters(int value);

    bool get_lr_deconv_deringing() const;
    void lr_deconv_deringing(bool enabled);

    bool get_unsh_mask_adaptive(int index) const;
    void unsh_mask_adaptive(int index, bool enabled);

    double get_unsh_mask_sigma(int index) const;
    void unsh_mask_sigma(int index, double value);

    double get_unsh_mask_amount_min(int index) const;
    void unsh_mask_amount_min(int index, double value);

    double get_unsh_mask_amount_max(int index) const;
    void unsh_mask_amount_max(int index, double value);

    double get_unsh_mask_amount(int index) const;
    void unsh_mask_amount(int index, double value);

    double get_unsh_mask_threshold(int index) const;
    void unsh_mask_threshold(int index, double value);

    double get_unsh_mask_twidth(int index) const;
    void unsh_mask_twidth(int index, double value);

    int tc_add_point(double x, double y);
    void tc_set_point(int index, double x, double y);

    static const luaL_Reg* GetMethods()
    {
        static const luaL_Reg methods[] = {
            {"get_normalization_enabled", [](lua_State* lua) {
                return ConstMethodBoolResult<SettingsWrapper>(lua, &SettingsWrapper::get_normalization_enabled);
            }},

            {"normalization_enabled", [](lua_State* lua) {
                return MethodBoolArg<SettingsWrapper>(lua, &SettingsWrapper::normalization_enabled);
            }},

            {"get_normalization_min", [](lua_State* lua) {
                return ConstMethodDoubleResult<SettingsWrapper>(lua, &SettingsWrapper::get_normalization_min);
            }},

            {"normalization_min", [](lua_State* lua) {
                return MethodDoubleArg<SettingsWrapper>(lua, &SettingsWrapper::normalization_min);
            }},

            {"get_normalization_max", [](lua_State* lua) {
                return ConstMethodDoubleResult<SettingsWrapper>(lua, &SettingsWrapper::get_normalization_max);
            }},

            {"normalization_max", [](lua_State* lua) {
                return MethodDoubleArg<SettingsWrapper>(lua, &SettingsWrapper::normalization_max);
            }},

            {"get_lr_deconv_sigma", [](lua_State* lua) {
                return ConstMethodDoubleResult<SettingsWrapper>(lua, &SettingsWrapper::get_lr_deconv_sigma);
            }},

            {"lr_deconv_sigma", [](lua_State* lua) {
                return MethodDoubleArg<SettingsWrapper>(lua, &SettingsWrapper::lr_deconv_sigma);
            }},

            {"get_lr_deconv_num_iters", [](lua_State* lua) {
                return ConstMethodIntResult<SettingsWrapper>(lua, &SettingsWrapper::get_lr_deconv_num_iters);
            }},

            {"lr_deconv_num_iters", [](lua_State* lua) {
                return MethodIntArg<SettingsWrapper>(lua, &SettingsWrapper::lr_deconv_num_iters);
            }},

            {"get_lr_deconv_deringing", [](lua_State* lua) {
                return ConstMethodBoolResult<SettingsWrapper>(lua, &SettingsWrapper::get_lr_deconv_deringing);
            }},

            {"lr_deconv_deringing", [](lua_State* lua) {
                return MethodBoolArg<SettingsWrapper>(lua, &SettingsWrapper::lr_deconv_deringing);
            }},

            {"get_unsh_mask_adaptive", [](lua_State* lua) {
                return ConstMethodIntArgBoolResult<SettingsWrapper>(lua, &SettingsWrapper::get_unsh_mask_adaptive);
            }},

            {"unsh_mask_adaptive", [](lua_State* lua) {
                return MethodIntBoolArg<SettingsWrapper>(lua, &SettingsWrapper::unsh_mask_adaptive);
            }},

            {"get_unsh_mask_sigma", [](lua_State* lua) {
                return ConstMethodIntArgDoubleResult<SettingsWrapper>(lua, &SettingsWrapper::get_unsh_mask_sigma);
            }},

            {"unsh_mask_sigma", [](lua_State* lua) {
                return MethodIntDoubleArg<SettingsWrapper>(lua, &SettingsWrapper::unsh_mask_sigma);
            }},

            {"get_unsh_mask_amount_min", [](lua_State* lua) {
                return ConstMethodIntArgDoubleResult<SettingsWrapper>(lua, &SettingsWrapper::get_unsh_mask_amount_min);
            }},

            {"unsh_mask_amount_min", [](lua_State* lua) {
                return MethodIntDoubleArg<SettingsWrapper>(lua, &SettingsWrapper::unsh_mask_amount_min);
            }},

            {"get_unsh_mask_amount_max", [](lua_State* lua) {
                return ConstMethodIntArgDoubleResult<SettingsWrapper>(lua, &SettingsWrapper::get_unsh_mask_amount_max);
            }},

            {"unsh_mask_amount_max", [](lua_State* lua) {
                return MethodIntDoubleArg<SettingsWrapper>(lua, &SettingsWrapper::unsh_mask_amount_max);
            }},

            {"get_unsh_mask_amount", [](lua_State* lua) {
                return ConstMethodIntArgDoubleResult<SettingsWrapper>(lua, &SettingsWrapper::get_unsh_mask_amount);
            }},

            {"unsh_mask_amount", [](lua_State* lua) {
                return MethodIntDoubleArg<SettingsWrapper>(lua, &SettingsWrapper::unsh_mask_amount);
            }},

            {"get_unsh_mask_threshold", [](lua_State* lua) {
                return ConstMethodIntArgDoubleResult<SettingsWrapper>(lua, &SettingsWrapper::get_unsh_mask_threshold);
            }},

            {"unsh_mask_threshold", [](lua_State* lua) {
                return MethodIntDoubleArg<SettingsWrapper>(lua, &SettingsWrapper::unsh_mask_threshold);
            }},

            {"get_unsh_mask_twidth", [](lua_State* lua) {
                return ConstMethodIntArgDoubleResult<SettingsWrapper>(lua, &SettingsWrapper::get_unsh_mask_twidth);
            }},

            {"unsh_mask_twidth", [](lua_State* lua) {
                return MethodIntDoubleArg<SettingsWrapper>(lua, &SettingsWrapper::unsh_mask_twidth);
            }},

            {"tc_add_point", [](lua_State* lua) {
                return MethodDoubleDoubleArgIntResult<SettingsWrapper>(lua, &SettingsWrapper::tc_add_point);
            }},

            {"tc_set_point", [](lua_State* lua) {
                return MethodIntDoubleDoubleArg<SettingsWrapper>(lua, &SettingsWrapper::tc_set_point);
            }},

            {nullptr, nullptr} // end-of-data marker
        };

        return methods;
    }

    const ProcessingSettings& GetSettings() const;

private:
    SettingsWrapper(const std::filesystem::path& path);

    ProcessingSettings m_Settings{};
};

}
