#pragma once
#include "slack/Types.h"
#include "ui/Theme.h"
#include <vector>
#include <functional>

namespace conduit::ui {

class BookmarkBar {
public:
    using OpenUrlCallback = std::function<void(const std::string& url)>;
    using RemoveCallback = std::function<void(const std::string& bookmark_id)>;

    void setBookmarks(const std::vector<slack::Bookmark>& bookmarks) { bookmarks_ = bookmarks; }
    bool hasBookmarks() const { return !bookmarks_.empty(); }
    void setOpenUrlCallback(OpenUrlCallback cb) { open_url_cb_ = std::move(cb); }
    void setRemoveCallback(RemoveCallback cb) { remove_cb_ = std::move(cb); }

    // returns height consumed (0 if no bookmarks)
    float render(float x, float y, float width, const Theme& theme);

private:
    std::vector<slack::Bookmark> bookmarks_;
    OpenUrlCallback open_url_cb_;
    RemoveCallback remove_cb_;
};

} // namespace conduit::ui
