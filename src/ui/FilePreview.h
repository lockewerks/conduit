#pragma once
#include "ui/Theme.h"
#include <string>

namespace conduit::ui {

// modal overlay for staring at images without squinting at the inline thumbnail
// eventually this could handle PDFs or whatever else slack lets people upload
class FilePreview {
public:
    struct TextureInfo {
        unsigned int texture_id = 0;
        int width = 0;
        int height = 0;
    };

    void open(const std::string& url, const TextureInfo& tex);
    void close();
    bool isOpen() const { return is_open_; }

    void render(float screen_w, float screen_h, const Theme& theme);

private:
    bool is_open_ = false;
    std::string url_;
    TextureInfo texture_;

    // scale the image to fit without making it comically large
    void fitToScreen(float screen_w, float screen_h, float& out_w, float& out_h) const;
};

} // namespace conduit::ui
