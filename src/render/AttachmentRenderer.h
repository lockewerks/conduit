#pragma once
#include "slack/Types.h"
#include "render/TextRenderer.h"
#include "render/ImageRenderer.h"
#include <imgui.h>
#include <vector>

namespace conduit::render {

class AttachmentRenderer {
public:
    void setImageRenderer(ImageRenderer* r) { image_renderer_ = r; }
    void setAuthToken(const std::string& t) { auth_token_ = t; }

    // render a list of attachments below a message
    float render(const std::vector<slack::Attachment>& attachments, float wrap_width,
                 const ImVec4& default_color);

private:
    ImageRenderer* image_renderer_ = nullptr;
    std::string auth_token_;

    ImU32 parseColor(const std::string& hex_color);
};

} // namespace conduit::render
