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
    Image wrapper implementation.
*/

#include "common/imppg_assert.h"
#include "alignment/align_proc.h"
#include "image/image.h"
#include "interop/classes/ImageWrapper.h"
#include "scripting/script_exceptions.h"

#include <boost/algorithm/string/case_conv.hpp>
#include <filesystem>

namespace scripting
{

ImageWrapper ImageWrapper::FromPath(const std::filesystem::path& imagePath)
{
    return ImageWrapper(imagePath);
}

ImageWrapper::ImageWrapper(const std::shared_ptr<const c_Image>& image)
: m_Image(image)
{}

ImageWrapper::ImageWrapper(const std::filesystem::path& imagePath)
{
    std::string internalErrorMsg;
    auto image = LoadImageFileAs32f(imagePath, false, &internalErrorMsg);
    if (!image.has_value())
    {
        auto message = std::string{"failed to load image from "} + imagePath.generic_string();
        if (!internalErrorMsg.empty()) { message += "; " + internalErrorMsg; }
        throw ScriptExecutionError(message);
    }

    m_Image = std::make_shared<c_Image>(std::move(image.value()));
}

ImageWrapper::ImageWrapper(c_Image&& image)
{
    m_Image = std::make_shared<c_Image>(std::move(image));
}

const std::shared_ptr<const c_Image>& ImageWrapper::GetImage() const
{
    return m_Image;
}

void ImageWrapper::save(const wxString& path, int outputFormat) const
{
    if (outputFormat < 0 || outputFormat >= static_cast<int>(OutputFormat::LAST))
    {
        throw ScriptExecutionError{"invalid output format"};
    }

    if (!m_Image->SaveToFile(ToFsPath(path), static_cast<OutputFormat>(outputFormat)))
    {
        throw ScriptExecutionError{wxString::Format(_("failed to save image as %s"), path).ToStdString()};
    }
}

void ImageWrapper::align_rgb()
{
    auto [red, green, blue] = m_Image->SplitRGB();

    const auto result = scripting::g_State->CallFunctionAndAwaitCompletion(
        scripting::contents::AlignRGB{std::move(red), std::move(green), std::move(blue)}
    );

    auto* alignedImg = std::get_if<call_result::ImageProcessed>(&result);
    IMPPG_ASSERT(alignedImg != nullptr);

    m_Image = std::make_shared<const c_Image>(alignedImg->image->GetConvertedPixelFormatSubImage(
        m_Image->GetPixelFormat(), 0, 0, m_Image->GetWidth(), m_Image->GetHeight()
    ));
}

void ImageWrapper::auto_white_balance()
{
    m_Image = std::make_shared<const c_Image>(m_Image->AutomaticWhiteBalance());
}

void ImageWrapper::multiply(double factor)
{
    if (factor < 0.0) { throw ScriptExecutionError{"negative factor to multiply pixel values"}; }

    auto multiplied = c_Image(*m_Image);
    multiplied.MultiplyPixelValues(factor);
    m_Image = std::make_shared<c_Image>(multiplied);
}

}
