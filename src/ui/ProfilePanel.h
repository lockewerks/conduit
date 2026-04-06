#pragma once
#include "slack/Types.h"
#include "ui/Theme.h"
#include "render/ImageRenderer.h"
#include <functional>
#include <string>

namespace conduit::ui {

class ProfilePanel {
public:
    using OpenDMCallback = std::function<void(const slack::UserId& user_id)>;

    void open(const slack::User& user);
    void close() { is_open_ = false; }
    bool isOpen() const { return is_open_; }
    const slack::UserId& userId() const { return user_.id; }

    void setUser(const slack::User& user) { user_ = user; }
    void setImageRenderer(render::ImageRenderer* r) { image_renderer_ = r; }
    void setAuthToken(const std::string& t) { auth_token_ = t; }
    void setOpenDMCallback(OpenDMCallback cb) { open_dm_cb_ = std::move(cb); }

    void render(float x, float y, float width, float height, const Theme& theme);

private:
    bool is_open_ = false;
    slack::User user_;
    render::ImageRenderer* image_renderer_ = nullptr;
    std::string auth_token_;
    OpenDMCallback open_dm_cb_;
};

} // namespace conduit::ui
