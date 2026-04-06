#pragma once
#include "slack/Types.h"
#include "ui/Theme.h"
#include <vector>
#include <functional>

namespace conduit::ui {

class ReminderPanel {
public:
    using CompleteCallback = std::function<void(const std::string& reminder_id)>;
    using DeleteCallback = std::function<void(const std::string& reminder_id)>;

    void open() { is_open_ = true; }
    void close() { is_open_ = false; }
    void toggle() { is_open_ = !is_open_; }
    bool isOpen() const { return is_open_; }

    void setReminders(const std::vector<slack::Reminder>& reminders) { reminders_ = reminders; }
    void setCompleteCallback(CompleteCallback cb) { complete_cb_ = std::move(cb); }
    void setDeleteCallback(DeleteCallback cb) { delete_cb_ = std::move(cb); }

    void render(float x, float y, float width, float height, const Theme& theme);

private:
    bool is_open_ = false;
    std::vector<slack::Reminder> reminders_;
    CompleteCallback complete_cb_;
    DeleteCallback delete_cb_;
};

} // namespace conduit::ui
