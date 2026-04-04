#include "ui/ThreadPanel.h"
#include <imgui.h>

namespace conduit::ui {

void ThreadPanel::open(const slack::ChannelId& channel, const slack::Timestamp& parent_ts) {
    channel_id_ = channel;
    parent_ts_ = parent_ts;
    is_open_ = true;
}

void ThreadPanel::close() {
    is_open_ = false;
    messages_.clear();
}

void ThreadPanel::addMessage(const slack::Message& message) {
    // insert in order
    auto pos = std::lower_bound(messages_.begin(), messages_.end(), message,
                                 [](const slack::Message& a, const slack::Message& b) {
                                     return a.ts < b.ts;
                                 });
    if (pos != messages_.end() && pos->ts == message.ts) {
        *pos = message;
    } else {
        messages_.insert(pos, message);
    }
}

void ThreadPanel::render(float x, float y, float width, float height, const Theme& theme) {
    if (!is_open_) return;

    ImGui::SetCursorPos({x, y});

    // separator line between main view and thread
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    dl->AddLine(p, {p.x, p.y + height}, ImGui::ColorConvertFloat4ToU32(theme.separator_line));

    ImGui::PushStyleColor(ImGuiCol_ChildBg, theme.bg_main);
    ImGui::BeginChild("##thread_panel", {width, height}, false);

    // header
    ImGui::PushStyleColor(ImGuiCol_Text, theme.text_bright);
    ImGui::TextUnformatted("Thread");
    ImGui::PopStyleColor();
    ImGui::Separator();

    // render thread messages
    for (auto& msg : messages_) {
        ImGui::PushStyleColor(ImGuiCol_Text, theme.text_dim);
        ImGui::TextUnformatted(msg.ts.substr(0, 5).c_str()); // crude time
        ImGui::PopStyleColor();
        ImGui::SameLine();

        ImGui::PushStyleColor(ImGuiCol_Text, theme.nickColor(msg.user));
        ImGui::TextUnformatted(msg.user.c_str());
        ImGui::PopStyleColor();
        ImGui::SameLine();

        ImGui::PushStyleColor(ImGuiCol_Text, theme.text_default);
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + width - 20);
        ImGui::TextUnformatted(msg.text.c_str());
        ImGui::PopTextWrapPos();
        ImGui::PopStyleColor();
    }

    if (messages_.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, theme.text_dim);
        ImGui::TextUnformatted("No replies yet. Be the change you want to see.");
        ImGui::PopStyleColor();
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

} // namespace conduit::ui
