#pragma once
#include "ui/Theme.h"
#include <string>
#include <vector>
#include <functional>

namespace conduit::ui {

// Alt+N overlay for hopping between slack orgs
// because apparently one workspace full of unread messages isn't enough
class OrgSwitcher {
public:
    struct OrgEntry {
        std::string team_id;
        std::string name;
        bool is_active = false;
    };

    using SelectCallback = std::function<void(const OrgEntry& org)>;

    void open();
    void close();
    bool isOpen() const { return is_open_; }

    void setOrgs(const std::vector<OrgEntry>& orgs) { orgs_ = orgs; }
    void setSelectCallback(SelectCallback cb) { select_cb_ = std::move(cb); }

    void render(float x, float y, float width, float height, const Theme& theme);

private:
    bool is_open_ = false;
    std::vector<OrgEntry> orgs_;
    int selected_ = 0;
    SelectCallback select_cb_;
};

} // namespace conduit::ui
