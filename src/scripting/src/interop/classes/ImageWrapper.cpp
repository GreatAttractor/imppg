#include "common/imppg_assert.h"
#include "alignment/align_proc.h"
#include "image/image.h"
#include "interop/classes/ImageWrapper.h"
#include "scripting/script_exceptions.h"

#include <boost/algorithm/string/case_conv.hpp>
#include <filesystem>

namespace scripting
{

ImageWrapper::ImageWrapper(const std::shared_ptr<const c_Image>& image)
: m_Image(image)
{}

ImageWrapper::ImageWrapper(const std::string& imagePath)
{
    std::string internalErrorMsg;
    auto image = LoadImageFileAs32f(imagePath, false, &internalErrorMsg);
    if (!image.has_value())
    {
        auto message = std::string{"failed to load image from "} + imagePath;
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

void ImageWrapper::save(const std::string& path, int outputFormat) const
{
    if (outputFormat < 0 || outputFormat >= static_cast<int>(OutputFormat::LAST))
    {
        throw ScriptExecutionError{"invalid output format"};
    }

    if (!m_Image->SaveToFile(path, static_cast<OutputFormat>(outputFormat)))
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
