#include "ui/FileBrowser.h"
#include <imgui.h>

namespace conduit::ui {

void FileBrowser::open(const slack::ChannelId& channel) {
    channel_id_ = channel;
    is_open_ = true;
    current_page_ = 1;
    needs_fetch_ = true;
}

void FileBrowser::setFiles(const std::vector<slack::SlackFile>& files, int total) {
    files_ = files;
    total_files_ = total;
    needs_fetch_ = false;
}

void FileBrowser::render(float x, float y, float width, float height, const Theme& theme) {
    if (!is_open_) return;

    if (needs_fetch_ && fetch_cb_) {
        auto files = fetch_cb_(channel_id_, 50, current_page_);
        // files set via setFiles callback
        needs_fetch_ = false;
    }

    ImGui::SetNextWindowPos(ImVec2(x, y));
    ImGui::SetNextWindowSize(ImVec2(width, height));
    ImGui::Begin("##file_browser", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);

    ImGui::PushStyleColor(ImGuiCol_Text, theme.text_bright);
    ImGui::Text("Files (%d)", total_files_);
    ImGui::PopStyleColor();
    ImGui::Separator();

    ImGui::BeginChild("##files_scroll", ImVec2(0, -30), false);
    for (auto& f : files_) {
        ImGui::PushID(f.id.c_str());

        ImGui::TextUnformatted(f.name.c_str());
        ImGui::SameLine();
        ImGui::TextDisabled("(%s, %d bytes)", f.mimetype.c_str(), f.size);

        if (!f.permalink.empty() && ImGui::IsItemClicked()) {
            // could open in browser or file preview
        }

        ImGui::Separator();
        ImGui::PopID();
    }
    ImGui::EndChild();

    // pagination
    if (current_page_ > 1) {
        if (ImGui::Button("< Prev")) {
            current_page_--;
            needs_fetch_ = true;
        }
        ImGui::SameLine();
    }
    int total_pages = (total_files_ + 49) / 50;
    ImGui::Text("Page %d / %d", current_page_, std::max(total_pages, 1));
    if (current_page_ < total_pages) {
        ImGui::SameLine();
        if (ImGui::Button("Next >")) {
            current_page_++;
            needs_fetch_ = true;
        }
    }

    ImGui::End();
}

} // namespace conduit::ui
