#include "interop/classes/ImageWrapper.h"
#include "scripting/script_exceptions.h"

#include <boost/algorithm/string/case_conv.hpp>
#include <filesystem>

namespace scripting
{

ImageWrapper::ImageWrapper(const std::string& imagePath)
{
    std::string internalErrorMsg;
    std::string extension = std::filesystem::path(imagePath).extension().generic_string();
    // remove the leading period
    if (extension.length() > 0) { extension = extension.substr(1); }
    boost::to_lower(extension);
    auto image = LoadImageFileAsMono32f(imagePath, extension, false, &internalErrorMsg);
    if (!image.has_value())
    {
        auto message = std::string{"failed to load image from "} + imagePath;
        if (!internalErrorMsg.empty()) { message += "; " + internalErrorMsg; }
        throw ScriptExecutionError(message);
    }

    m_Image = std::make_shared<c_Image>(std::move(image.value()));
}

const std::shared_ptr<const c_Image>& ImageWrapper::GetImage() const
{
    return m_Image;
}

}
