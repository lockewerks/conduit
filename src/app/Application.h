#pragma once
#include "ui/UIManager.h"
#include "app/Config.h"
#include "slack/SlackClient.h"
#include "cache/MessageCache.h"
#include "input/CommandParser.h"
#include "input/TabComplete.h"
#include "input/InputHistory.h"
#include "input/KeyHandler.h"
#include "notify/NotificationManager.h"
#include "util/ThreadPool.h"
#include "render/ImageRenderer.h"
#include "render/GifRenderer.h"
#include "render/EmojiRenderer.h"

#include <SDL.h>
#include <chrono>
#include <memory>

typedef void* SDL_GLContext;

namespace conduit {

// the big boss. owns everything, wires everything together.
class Application {
public:
    Application();
    ~Application();

    bool init();
    void run();
    void shutdown();

private:
    bool initSDL();
    bool initOpenGL();
    bool initImGui();
    void loadFonts();
    void loadConfig();
    void connectToSlack();
    void registerCommands();
    void setupKeybindings();
    void setupTabCompletion();

    void processInput();
    void processSlackEvents();
    void handleKeyDown(const SDL_KeyboardEvent& key);
    void handleInputSubmit(const std::string& text);
    bool tryPasteClipboardImage();

    // update the UI from real slack data
    void syncBufferList();
    void syncBufferView();
    void syncNickList();
    void switchToBuffer(int index);

    SDL_Window* window_ = nullptr;
    SDL_GLContext gl_context_ = nullptr;
    bool running_ = false;
    int window_width_ = 1280;
    int window_height_ = 800;

    // the pieces
    Config config_;
    ui::UIManager ui_;
    std::unique_ptr<slack::SlackClient> client_;
    std::unique_ptr<cache::Database> db_;
    std::unique_ptr<cache::MessageCache> msg_cache_;
    std::unique_ptr<ThreadPool> pool_;
    input::CommandParser commands_;
    input::TabComplete tab_complete_;
    input::InputHistory input_history_;
    input::KeyHandler keys_;

    // renderers for the fancy stuff
    render::ImageRenderer image_renderer_;
    render::GifRenderer gif_renderer_;
    render::EmojiRenderer emoji_renderer_;
    float last_frame_time_ = 0.0f;

    // current state
    slack::ChannelId active_channel_;
    std::string active_channel_name_;
    bool needs_channel_sync_ = true;
    bool needs_message_sync_ = true;

    // typing indicators from other users
    std::vector<std::pair<std::string, std::chrono::steady_clock::time_point>> typing_events_;

    // desktop notifications for mentions and DMs
    notify::NotificationManager notifications_;

    // token prompt state (for first-run)
    bool awaiting_token_ = false;
    std::string token_prompt_field_;
};

} // namespace conduit
