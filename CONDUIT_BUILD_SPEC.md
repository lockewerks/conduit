# CONDUIT — Build Specification
## A WeeChat-Style Slack Client Built on C++/Dear ImGui

**Version:** 1.0.0-draft
**Author:** Archon C. Locke / Locke Werks
**Target:** Claude Code autonomous build

---

## Table of Contents

1. [Project Overview](#1-project-overview)
2. [Technology Stack](#2-technology-stack)
3. [Directory Structure](#3-directory-structure)
4. [Build System](#4-build-system)
5. [Architecture Overview](#5-architecture-overview)
6. [Core Classes & Interfaces](#6-core-classes--interfaces)
7. [Slack API Integration](#7-slack-api-integration)
8. [UI Layout & Rendering](#8-ui-layout--rendering)
9. [Media Pipeline](#9-media-pipeline)
10. [SQLite Schema](#10-sqlite-schema)
11. [Keybindings & Commands](#11-keybindings--commands)
12. [Configuration System](#12-configuration-system)
13. [Multi-Org Support](#13-multi-org-support)
14. [Security Considerations](#14-security-considerations)
15. [Phased Build Plan](#15-phased-build-plan)
16. [Testing Strategy](#16-testing-strategy)
17. [Known Challenges & Mitigations](#17-known-challenges--mitigations)

---

## 1. Project Overview

**Conduit** is a desktop Slack client that looks and behaves like a terminal-based IRC client (specifically WeeChat) but is actually a C++ GUI application built on Dear ImGui. It renders text in a monospace font with WeeChat-style layout, colors, and buffer management — but unlike a true TUI, it can natively render images, GIFs, emoji, and reaction badges inline within the message flow.

### Design Philosophy

- **Terminal aesthetic, GUI capabilities.** Every pixel should look like it belongs in a terminal. Images and GIFs are the *only* exception, and even they should feel embedded naturally within the text flow (think inline thumbnails with defined max-widths, not floating cards).
- **Keyboard-first.** Every action must be reachable via keyboard. Mouse support exists but is secondary.
- **WeeChat muscle memory.** Users who know WeeChat should feel at home. Buffer navigation, command syntax, and layout conventions follow WeeChat patterns.
- **Full Slack feature parity.** This replaces the Slack desktop client entirely. If Slack supports it, Conduit supports it. No feature should require falling back to the browser.
- **Multi-org from day one.** The architecture treats each Slack workspace as an independent client instance. Switching between orgs is as natural as switching WeeChat servers.

### Non-Goals

- Conduit is NOT a general IRC client. It speaks Slack's API exclusively.
- Conduit does NOT use Electron, web views, or browser engines.
- Conduit does NOT attempt to replicate Slack's visual design language.

---

## 2. Technology Stack

### Core

| Component | Technology | Version | Purpose |
|-----------|-----------|---------|---------|
| Language | C++20 | — | Core application |
| UI Framework | Dear ImGui | docking branch (latest) | Immediate-mode GUI rendering |
| Windowing | SDL2 | 2.28+ | Cross-platform window/input management |
| Rendering | OpenGL 3.3+ | — | GPU-accelerated rendering, texture support |
| JSON | nlohmann/json | 3.11+ | Slack API JSON parsing |
| HTTP | libcurl | 7.80+ | REST API calls, file downloads |
| WebSocket | libwebsockets (lws) | 4.3+ | Slack Socket Mode real-time events |
| Database | SQLite3 | 3.40+ | Local message/channel/user cache |
| Image Decode | stb_image | latest | PNG/JPEG decoding to RGBA |
| GIF Decode | stb_image (for static) + giflib | 5.2+ | Animated GIF frame decoding |
| Font | Monospace TTF | — | JetBrains Mono or Iosevka (bundled) |
| Emoji | Emoji spritesheet | — | Slack emoji rendered as textures |
| Markdown | Custom parser | — | Slack mrkdwn to ImGui rich text |
| Crypto | OS keychain API | — | Token storage (macOS Keychain, Windows DPAPI, libsecret on Linux) |

### Build

| Tool | Purpose |
|------|---------|
| CMake 3.24+ | Build system |
| vcpkg or FetchContent | Dependency management |
| clang-format | Code formatting |
| clang-tidy | Static analysis |

### Platform Targets

| Platform | Priority | Notes |
|----------|----------|-------|
| Windows (x86_64) | P0 | Primary dev platform, MSVC preferred |
| Linux (x86_64) | P1 | Secondary target |
| macOS (arm64/x86_64) | P2 | Universal binary |

---

## 3. Directory Structure

```
conduit/
├── CMakeLists.txt
├── README.md
├── LICENSE
├── .clang-format
├── .clang-tidy
├── .gitignore
├── vcpkg.json                    # vcpkg manifest
│
├── assets/
│   ├── fonts/
│   │   ├── JetBrainsMono-Regular.ttf
│   │   ├── JetBrainsMono-Bold.ttf
│   │   └── JetBrainsMono-Italic.ttf
│   ├── emoji/
│   │   └── emoji_spritesheet.png  # Slack-compatible emoji atlas
│   ├── icons/
│   │   └── conduit.png            # App icon
│   └── themes/
│       ├── weechat_dark.toml      # Default theme
│       └── weechat_light.toml
│
├── config/
│   └── conduit.example.toml       # Example config
│
├── src/
│   ├── main.cpp                   # Entry point, SDL2/OpenGL init, main loop
│   │
│   ├── app/
│   │   ├── Application.h/.cpp     # Top-level app lifecycle
│   │   ├── Config.h/.cpp          # TOML config parser
│   │   └── KeychainStore.h/.cpp   # Platform-specific token storage
│   │
│   ├── slack/
│   │   ├── SlackClient.h/.cpp     # Per-org Slack API client (REST + Socket Mode)
│   │   ├── SlackAuth.h/.cpp       # OAuth2 flow handling
│   │   ├── SocketModeClient.h/.cpp # WebSocket connection manager
│   │   ├── WebAPI.h/.cpp          # libcurl REST wrapper for Slack Web API
│   │   ├── EventDispatcher.h/.cpp # Routes Slack events to handlers
│   │   ├── RateLimiter.h/.cpp     # Per-method rate limit tracker (Tier 1-4)
│   │   └── Types.h                # Slack data types: Message, Channel, User, Reaction, File, etc.
│   │
│   ├── cache/
│   │   ├── Database.h/.cpp        # SQLite wrapper, migrations
│   │   ├── MessageCache.h/.cpp    # Message storage/retrieval
│   │   ├── ChannelCache.h/.cpp    # Channel list cache
│   │   ├── UserCache.h/.cpp       # User/presence cache
│   │   └── FileCache.h/.cpp       # Downloaded media file cache (disk-backed)
│   │
│   ├── ui/
│   │   ├── UIManager.h/.cpp       # Root UI layout orchestrator
│   │   ├── Theme.h/.cpp           # WeeChat color scheme, ImGui style
│   │   ├── BufferView.h/.cpp      # Single chat buffer (channel/DM/thread)
│   │   ├── BufferList.h/.cpp      # Left sidebar: channel/buffer list
│   │   ├── NickList.h/.cpp        # Right sidebar: member list
│   │   ├── InputBar.h/.cpp        # Bottom input with slash commands, completion
│   │   ├── StatusBar.h/.cpp       # Bottom status line (connection, org, typing)
│   │   ├── TitleBar.h/.cpp        # Top bar: topic, channel info
│   │   ├── ThreadPanel.h/.cpp     # Thread side panel (opens right of buffer)
│   │   ├── SearchPanel.h/.cpp     # Ctrl+F message search overlay
│   │   ├── OrgSwitcher.h/.cpp     # Alt+N org switching overlay
│   │   ├── FilePreview.h/.cpp     # Modal/overlay for full-size image/file view
│   │   ├── EmojiPicker.h/.cpp     # Emoji selection popup (searchable grid)
│   │   ├── NotificationPopup.h/.cpp # Desktop notification overlay
│   │   └── CommandPalette.h/.cpp  # Ctrl+K fuzzy command/channel finder
│   │
│   ├── render/
│   │   ├── TextRenderer.h/.cpp    # Rich text layout: mrkdwn -> ImGui calls
│   │   ├── ImageRenderer.h/.cpp   # Image texture loading, scaling, display
│   │   ├── GifRenderer.h/.cpp     # Animated GIF frame management
│   │   ├── EmojiRenderer.h/.cpp   # Emoji atlas UV lookup + rendering
│   │   ├── ReactionBadge.h/.cpp   # Inline reaction rendering [:emoji: N]
│   │   ├── CodeBlock.h/.cpp       # Syntax-highlighted code blocks
│   │   └── LinkHandler.h/.cpp     # Clickable URLs, channel refs, user mentions
│   │
│   ├── input/
│   │   ├── KeyHandler.h/.cpp      # Global key dispatch, modal key contexts
│   │   ├── CommandParser.h/.cpp   # /slash command parsing and routing
│   │   ├── TabComplete.h/.cpp     # @user, #channel, :emoji: tab completion
│   │   └── InputHistory.h/.cpp    # Per-buffer up/down input history
│   │
│   ├── notify/
│   │   ├── NotificationManager.h/.cpp  # Cross-platform desktop notification dispatch
│   │   └── SoundManager.h/.cpp         # Optional notification sounds
│   │
│   └── util/
│       ├── ThreadPool.h/.cpp      # Async task execution for API calls
│       ├── Logger.h/.cpp          # Structured logging (file + debug buffer)
│       ├── TimeFormat.h/.cpp      # Timestamp formatting (relative/absolute)
│       ├── Unicode.h/.cpp         # UTF-8 utilities
│       └── Platform.h/.cpp        # OS detection, path resolution
│
├── tests/
│   ├── CMakeLists.txt
│   ├── test_mrkdwn_parser.cpp
│   ├── test_command_parser.cpp
│   ├── test_cache.cpp
│   ├── test_rate_limiter.cpp
│   └── test_types.cpp
│
└── docs/
    ├── ARCHITECTURE.md
    ├── KEYBINDINGS.md
    └── SLASH_COMMANDS.md
```

---

## 4. Build System

### CMakeLists.txt (Root)

```cmake
cmake_minimum_required(VERSION 3.24)
project(conduit VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Options
option(CONDUIT_BUILD_TESTS "Build test suite" ON)
option(CONDUIT_USE_VCPKG "Use vcpkg for dependencies" ON)

# Dependencies via vcpkg or find_package
find_package(SDL2 REQUIRED)
find_package(OpenGL REQUIRED)
find_package(CURL REQUIRED)
find_package(SQLite3 REQUIRED)
find_package(Libwebsockets REQUIRED)
find_package(nlohmann_json REQUIRED)

# FetchContent for header-only / small deps
include(FetchContent)

FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG docking
)
FetchContent_MakeAvailable(imgui)

FetchContent_Declare(stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG master
)
FetchContent_MakeAvailable(stb)

FetchContent_Declare(tomlplusplus
    GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
    GIT_TAG v3.4.0
)
FetchContent_MakeAvailable(tomlplusplus)

# Dear ImGui as a static library (SDL2 + OpenGL3 backends)
add_library(imgui_lib STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui_lib PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)
target_link_libraries(imgui_lib PUBLIC SDL2::SDL2 OpenGL::GL)

# Main executable
file(GLOB_RECURSE CONDUIT_SOURCES src/*.cpp)
add_executable(conduit ${CONDUIT_SOURCES})

target_include_directories(conduit PRIVATE
    src/
    ${stb_SOURCE_DIR}
)

target_link_libraries(conduit PRIVATE
    imgui_lib
    SDL2::SDL2
    SDL2::SDL2main
    OpenGL::GL
    CURL::libcurl
    SQLite::SQLite3
    websockets
    nlohmann_json::nlohmann_json
    tomlplusplus::tomlplusplus
)

# Asset installation
file(COPY assets/ DESTINATION ${CMAKE_BINARY_DIR}/assets/)
file(COPY config/ DESTINATION ${CMAKE_BINARY_DIR}/config/)

# Tests
if(CONDUIT_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
```

### vcpkg.json

```json
{
  "name": "conduit",
  "version-string": "0.1.0",
  "dependencies": [
    "sdl2",
    "curl",
    "sqlite3",
    "libwebsockets",
    "nlohmann-json",
    "giflib"
  ]
}
```

### Build Commands

```bash
# Configure
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake

# Build
cmake --build build -j$(nproc)

# Run
./build/conduit

# Test
cd build && ctest --output-on-failure
```

---

## 5. Architecture Overview

### Data Flow

```
┌─────────────────────────────────────────────────────────┐
│                    Application Layer                     │
│  ┌──────────┐  ┌──────────┐  ┌────────────────────────┐ │
│  │ UIManager │  │KeyHandler│  │  CommandParser          │ │
│  └─────┬────┘  └────┬─────┘  └───────────┬────────────┘ │
│        │            │                     │              │
│  ┌─────▼────────────▼─────────────────────▼────────────┐ │
│  │              BufferView (per channel/DM)             │ │
│  │  ┌──────────┐ ┌────────────┐ ┌───────────────────┐  │ │
│  │  │TextRender│ │ImageRender │ │ ReactionBadge     │  │ │
│  │  └──────────┘ └────────────┘ └───────────────────┘  │ │
│  └─────────────────────┬───────────────────────────────┘ │
│                        │                                 │
│  ┌─────────────────────▼───────────────────────────────┐ │
│  │              SlackClient (per org)                   │ │
│  │  ┌────────────┐  ┌──────────┐  ┌──────────────────┐ │ │
│  │  │SocketMode  │  │ Web API  │  │  RateLimiter     │ │ │
│  │  │ (realtime) │  │ (REST)   │  │  (per-method)    │ │ │
│  │  └──────┬─────┘  └────┬─────┘  └──────────────────┘ │ │
│  └─────────┼─────────────┼─────────────────────────────┘ │
│            │             │                               │
│  ┌─────────▼─────────────▼─────────────────────────────┐ │
│  │     EventDispatcher → Cache (SQLite) → UI Update    │ │
│  └─────────────────────────────────────────────────────┘ │
│                                                          │
│  ┌─────────────────────────────────────────────────────┐ │
│  │              ThreadPool (async I/O)                  │ │
│  │  • API calls   • File downloads   • Image decode    │ │
│  └─────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────┘
```

### Threading Model

- **Main thread:** SDL event loop + ImGui rendering. ALL ImGui calls happen here. NEVER call ImGui from worker threads.
- **Socket Mode thread:** One per org. Reads WebSocket, parses events, pushes to a thread-safe event queue consumed by main thread.
- **Thread pool (4-8 workers):** HTTP requests (Web API calls, file downloads), image decoding, SQLite writes. Results posted back to main thread via a lock-free queue.
- **Synchronization:** Use `std::mutex` + `std::condition_variable` for the event queue. Use `std::atomic` for simple flags (connection status, typing indicators). Consider `moodycamel::ConcurrentQueue` for the lock-free queue if contention becomes measurable.

### Ownership Model

```
Application (1)
  └── OrgManager (1)
        └── SlackClient (1 per org)
              ├── SocketModeClient (1)
              ├── WebAPI (1, shared libcurl multi-handle)
              ├── EventDispatcher (1)
              ├── RateLimiter (1)
              └── Database (1 per org, separate .db file)

UIManager (1)
  ├── BufferList (1)
  ├── BufferView (N, one per open channel/DM/thread)
  ├── NickList (1, context-sensitive to active buffer)
  ├── InputBar (1)
  ├── StatusBar (1)
  ├── TitleBar (1)
  ├── ThreadPanel (0-1)
  ├── SearchPanel (0-1)
  ├── OrgSwitcher (0-1)
  ├── EmojiPicker (0-1)
  ├── CommandPalette (0-1)
  └── FilePreview (0-1)
```

---

## 6. Core Classes & Interfaces

### 6.1 Slack Types (`src/slack/Types.h`)

All Slack data types as C++ structs. These are the canonical in-memory representations.

```cpp
#pragma once
#include <string>
#include <vector>
#include <optional>
#include <chrono>
#include <nlohmann/json.hpp>

namespace conduit::slack {

using Timestamp = std::string;  // Slack's "1234567890.123456" format
using UserId = std::string;     // U-prefixed
using ChannelId = std::string;  // C-prefixed (channels), D-prefixed (DMs), G-prefixed (groups)
using TeamId = std::string;     // T-prefixed
using FileId = std::string;

enum class ChannelType {
    PublicChannel,
    PrivateChannel,
    DirectMessage,
    MultiPartyDM,
    GroupDM
};

struct User {
    UserId id;
    std::string display_name;
    std::string real_name;
    std::string avatar_url_72;   // 72px avatar for nick list
    std::string avatar_url_192;  // 192px for profile view
    std::string status_emoji;
    std::string status_text;
    bool is_bot = false;
    bool is_online = false;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(User, id, display_name, real_name,
        avatar_url_72, avatar_url_192, status_emoji, status_text, is_bot, is_online)
};

struct Reaction {
    std::string emoji_name;  // without colons, e.g. "thumbsup"
    int count = 0;
    std::vector<UserId> users;
    bool user_reacted = false;  // did the current authed user react with this
};

struct Attachment {
    std::string fallback;
    std::string color;       // hex without #
    std::string title;
    std::string title_link;
    std::string text;
    std::string image_url;
    int image_width = 0;
    int image_height = 0;
};

struct SlackFile {
    FileId id;
    std::string name;
    std::string mimetype;
    std::string url_private;       // requires auth to download
    std::string thumb_360;         // thumbnail URL
    std::string thumb_480;
    int size = 0;                  // bytes
    int original_w = 0;
    int original_h = 0;
    std::string permalink;
};

struct Message {
    Timestamp ts;                       // primary key
    Timestamp thread_ts;                // if reply, points to parent
    UserId user;
    std::string text;                   // raw mrkdwn
    std::string subtype;                // "", "bot_message", "channel_join", etc.
    std::vector<Reaction> reactions;
    std::vector<Attachment> attachments;
    std::vector<SlackFile> files;
    bool is_edited = false;
    Timestamp edited_ts;
    int reply_count = 0;               // only on parent messages
    std::vector<UserId> reply_users;   // first N reply participants
    bool is_pinned = false;

    // Computed at render time, not stored
    mutable float rendered_height = 0.0f;
};

struct Channel {
    ChannelId id;
    std::string name;
    std::string topic;
    std::string purpose;
    ChannelType type;
    bool is_member = false;
    bool is_muted = false;
    bool is_archived = false;
    int unread_count = 0;
    bool has_unreads = false;
    Timestamp last_read;
    int member_count = 0;

    // For DMs
    UserId dm_user_id;

    // UI state (not persisted)
    bool is_open_in_buffer = false;
};

struct ThreadInfo {
    Timestamp parent_ts;
    ChannelId channel_id;
    int reply_count = 0;
    std::vector<UserId> participants;
    Timestamp latest_reply;
};

struct TypingEvent {
    ChannelId channel;
    UserId user;
    std::chrono::steady_clock::time_point expires;
};

} // namespace conduit::slack
```

### 6.2 SlackClient Interface

```cpp
class SlackClient {
public:
    explicit SlackClient(const OrgConfig& config, Database& db);
    ~SlackClient();

    // Lifecycle
    bool connect();
    void disconnect();
    bool isConnected() const;
    TeamId teamId() const;
    std::string teamName() const;

    // Channel operations
    std::vector<Channel> getChannels(bool include_archived = false);
    std::optional<Channel> getChannel(const ChannelId& id);
    bool joinChannel(const ChannelId& id);
    bool leaveChannel(const ChannelId& id);
    bool markRead(const ChannelId& channel, const Timestamp& ts);
    bool setTopic(const ChannelId& channel, const std::string& topic);

    // Message operations
    std::vector<Message> getHistory(const ChannelId& channel, int limit = 100,
                                     const std::optional<Timestamp>& before = std::nullopt);
    std::vector<Message> getThreadReplies(const ChannelId& channel, const Timestamp& thread_ts);
    bool sendMessage(const ChannelId& channel, const std::string& text,
                     const std::optional<Timestamp>& thread_ts = std::nullopt);
    bool editMessage(const ChannelId& channel, const Timestamp& ts, const std::string& new_text);
    bool deleteMessage(const ChannelId& channel, const Timestamp& ts);
    bool addReaction(const ChannelId& channel, const Timestamp& ts, const std::string& emoji);
    bool removeReaction(const ChannelId& channel, const Timestamp& ts, const std::string& emoji);

    // File operations
    bool uploadFile(const ChannelId& channel, const std::string& filepath,
                    const std::optional<std::string>& title = std::nullopt);
    std::vector<uint8_t> downloadFile(const std::string& url);

    // User operations
    std::optional<User> getUser(const UserId& id);
    std::vector<User> getChannelMembers(const ChannelId& channel);

    // Search
    struct SearchResult { Message message; ChannelId channel; std::string channel_name; };
    std::vector<SearchResult> searchMessages(const std::string& query, int count = 20);

    // Pins & bookmarks
    std::vector<Message> getPins(const ChannelId& channel);
    bool pinMessage(const ChannelId& channel, const Timestamp& ts);
    bool unpinMessage(const ChannelId& channel, const Timestamp& ts);

    // Typing
    void sendTyping(const ChannelId& channel);

    // User status
    bool setStatus(const std::string& emoji, const std::string& text, int expiration_minutes = 0);
    bool setPresence(bool is_away);

    // Event queue (consumed by main thread)
    bool pollEvent(SlackEvent& event_out);

private:
    std::unique_ptr<SocketModeClient> socket_;
    std::unique_ptr<WebAPI> api_;
    std::unique_ptr<RateLimiter> rate_limiter_;
    std::unique_ptr<EventDispatcher> dispatcher_;
    Database& db_;
    OrgConfig config_;
    ThreadSafeQueue<SlackEvent> event_queue_;
};
```

### 6.3 SlackEvent Union

```cpp
// Tagged union for all Slack events the UI cares about
struct SlackEvent {
    enum class Type {
        MessageNew,
        MessageChanged,
        MessageDeleted,
        ReactionAdded,
        ReactionRemoved,
        ChannelMarked,
        ChannelJoined,
        ChannelLeft,
        ChannelCreated,
        ChannelRenamed,
        ChannelArchived,
        MemberJoined,
        MemberLeft,
        PresenceChange,
        TypingStart,
        FileShared,
        PinAdded,
        PinRemoved,
        UserChange,
        TeamJoin,
        ConnectionStatus,   // connected, disconnected, reconnecting
    };

    Type type;
    ChannelId channel;
    UserId user;
    Timestamp ts;
    Timestamp thread_ts;

    // Payload varies by type — use std::variant or just optional fields
    std::optional<Message> message;
    std::optional<Reaction> reaction;
    std::optional<Channel> channel_info;
    std::optional<User> user_info;
    std::optional<bool> is_online;
    std::optional<std::string> connection_state;  // "connected", "reconnecting", "disconnected"
};
```

### 6.4 BufferView

The core UI element. Each open channel/DM is a BufferView. This is the most complex class.

```cpp
class BufferView {
public:
    BufferView(const ChannelId& channel, SlackClient& client, Database& db);

    void render(float x, float y, float width, float height);
    void onEvent(const SlackEvent& event);

    // Scrolling
    void scrollToBottom();
    void scrollUp(int lines);
    void scrollDown(int lines);
    void pageUp();
    void pageDown();
    bool isScrolledToBottom() const;

    // Message interaction
    void selectMessage(int index);       // for reaction/reply/edit context
    int selectedMessageIndex() const;
    const Message* selectedMessage() const;

    // History loading
    void loadMoreHistory();              // triggered when scrolling to top
    bool isLoadingHistory() const;

    // State
    ChannelId channelId() const;
    std::string displayName() const;
    int unreadCount() const;
    bool hasUnreads() const;
    bool hasMention() const;

private:
    ChannelId channel_id_;
    SlackClient& client_;
    Database& db_;

    std::vector<Message> messages_;      // sorted by ts ascending
    int scroll_offset_ = 0;             // lines from bottom
    bool auto_scroll_ = true;           // stick to bottom on new messages
    bool loading_history_ = false;
    int selected_index_ = -1;

    // Render cache
    struct RenderLine {
        enum class Type { Timestamp, Nickname, Text, Image, Reaction, CodeBlock, Separator, SystemMsg };
        Type type;
        std::string text;
        ImVec4 color;
        int message_index;               // back-reference to source message
        GLuint texture_id = 0;           // for image lines
        float image_width = 0;
        float image_height = 0;
    };
    std::vector<RenderLine> render_lines_;  // flattened render output
    bool render_dirty_ = true;              // re-layout on next frame

    void rebuildRenderLines();
    void renderLine(const RenderLine& line, float x, float y, float width);
};
```

---

## 7. Slack API Integration

### 7.1 Authentication

Conduit uses **Slack App with Bot + User tokens**. The user creates a Slack App in their workspace and provides the tokens via config.

**Required scopes (User Token — `xoxp-`):**
```
channels:read channels:write channels:history
groups:read groups:write groups:history
im:read im:write im:history
mpim:read mpim:write mpim:history
users:read users:read.email users:write
reactions:read reactions:write
pins:read pins:write
files:read files:write
search:read
emoji:read
chat:write
usergroups:read
```

**Required scopes (Bot Token — `xoxb-`):**
```
(only needed if using Socket Mode app-level token)
connections:write
```

**App-Level Token (`xapp-`):**
Required for Socket Mode. Created under the app's "Basic Information" page with `connections:write` scope.

### 7.2 Socket Mode (Real-Time Events)

Socket Mode replaces the deprecated RTM API. It uses WebSocket but through Slack's Socket Mode protocol.

**Connection flow:**
1. POST `https://slack.com/api/apps.connections.open` with `xapp-` token → get `wss://` URL
2. Connect WebSocket to that URL
3. Receive `hello` envelope
4. For each event envelope received:
   - Parse the `envelope_id`
   - **Immediately** send acknowledgment: `{"envelope_id": "<id>"}`
   - Process the event payload
5. Handle disconnection with exponential backoff reconnect (1s, 2s, 4s, 8s, max 30s)

**Envelope format:**
```json
{
  "envelope_id": "abc123",
  "type": "events_api",
  "payload": {
    "event": {
      "type": "message",
      "channel": "C1234",
      "user": "U5678",
      "text": "hello world",
      "ts": "1234567890.123456"
    }
  }
}
```

**Events to subscribe to (in Slack App config):**
```
message.channels
message.groups
message.im
message.mpim
reaction_added
reaction_removed
channel_created
channel_rename
channel_archive
channel_unarchive
member_joined_channel
member_left_channel
user_change
team_join
pin_added
pin_removed
file_shared
presence_change (requires presence subscription)
```

### 7.3 Web API (REST)

All REST calls go through `WebAPI` class which wraps libcurl.

**Implementation requirements:**
- Use a `CURLM` multi-handle for async requests via the thread pool
- Set `Authorization: Bearer xoxp-...` header on every request
- Parse `Retry-After` headers and respect them
- All POST bodies are `application/x-www-form-urlencoded` OR `application/json` depending on method
- File uploads use `multipart/form-data`
- Parse pagination cursors from `response_metadata.next_cursor`

**Rate limiting by tier:**

| Tier | Rate | Methods |
|------|------|---------|
| 1 | ~1 req/sec | `chat.postMessage`, `chat.update`, `chat.delete`, `reactions.add`, `reactions.remove` |
| 2 | ~20 req/min | `conversations.history`, `conversations.replies`, `conversations.members` |
| 3 | ~50 req/min | `conversations.list`, `users.list`, `search.messages` |
| 4 | ~100+ req/min | `conversations.info`, `users.info`, `emoji.list`, `auth.test` |
| Special | ~1 req/3s | `files.upload` |

Note: These are approximate. Slack uses a token-bucket algorithm internally, so short bursts above the rate are tolerated. The `RateLimiter` should primarily handle 429 responses gracefully (respect `Retry-After` header) rather than aggressively pre-throttling.

The `RateLimiter` class must:
- Track per-method request timestamps with a sliding window
- Reactively handle 429 responses by respecting the `Retry-After` header
- Queue and retry requests that receive 429s with appropriate backoff
- Optionally pre-throttle if approaching known tier limits (soft prevention)
- Expose `recordCall(method)`, `shouldThrottle(method)`, and `retryAfter(method)` interfaces
- Log rate limit events for debugging

### 7.4 Key API Methods

```
# Bootstrap (on connect)
auth.test                    → verify token, get user/team info
conversations.list           → fetch all channels user is in (paginated)
users.list                   → fetch all workspace users (paginated)
emoji.list                   → fetch custom emoji map

# Ongoing
conversations.history        → load channel message history (paginated)
conversations.replies        → load thread replies
conversations.mark           → mark channel as read
conversations.info           → get channel details
chat.postMessage             → send message
chat.update                  → edit message
chat.delete                  → delete message
reactions.add                → add reaction
reactions.remove             → remove reaction
files.upload                 → upload file
pins.add / pins.remove       → pin management
pins.list                    → get pinned messages
search.messages              → search messages
users.info                   → get single user info
users.getPresence            → get user presence
users.profile.set            → set own status
```

---

## 8. UI Layout & Rendering

### 8.1 WeeChat Layout

```
┌─────────────────────────────────────────────────────────────────┐
│ #general | Conduit v0.1.0 | Topic: Welcome to the team!        │  ← TitleBar
├──────────┬─────────────────────────────────────────┬────────────┤
│          │                                         │            │
│ Servers  │  10:30  alice  | hey everyone           │ @alice  ● │
│ ──────── │  10:31  bob    | morning! check this:   │ @bob    ● │
│ OrgName  │               | [image: screenshot.png] │ @carol  ○ │
│  #general│               | 📎 320x240              │ @dave   ○ │
│  #random │  10:32  carol  | nice [:+1: 3] [:❤️: 1] │            │
│  #dev    │  10:33  dave   | `/deploy staging`      │            │
│  @alice  │  10:35  ───── new messages ─────        │            │
│  @bob    │  10:36  alice  | @bob did you see       │            │
│          │               | the PR?                 │            │
│          │  10:37  bot   | Build #1234 passed ✅    │            │
│          │                                         │            │
├──────────┴─────────────────────────────────────────┴────────────┤
│ [@alice] #general > _                                           │  ← InputBar
├─────────────────────────────────────────────────────────────────┤
│ [conduit] [OrgName] [connected] [3 unread]              12:00  │  ← StatusBar
└─────────────────────────────────────────────────────────────────┘
```

### 8.2 Layout Dimensions (Dear ImGui)

```cpp
// Layout constants (configurable via theme)
struct LayoutConfig {
    float buffer_list_width = 180.0f;      // left sidebar
    float nick_list_width = 160.0f;        // right sidebar
    float input_bar_height = 0.0f;         // auto-sized, min 1 line
    float status_bar_height = 20.0f;       // single line
    float title_bar_height = 24.0f;        // single line
    float thread_panel_width = 400.0f;     // right side panel when viewing thread
    float message_padding_x = 8.0f;
    float message_padding_y = 2.0f;
    float timestamp_width = 54.0f;         // "HH:MM " fixed width
    float nickname_width = 120.0f;         // right-aligned, truncated
    float image_max_width = 360.0f;        // inline image max width in pixels
    float image_max_height = 240.0f;       // inline image max height
    float reaction_badge_height = 18.0f;
    float emoji_size = 16.0f;              // inline emoji render size

    // Sidebar toggles
    bool show_buffer_list = true;
    bool show_nick_list = true;
};
```

### 8.3 WeeChat Color Theme

```cpp
struct Theme {
    // Background
    ImVec4 bg_main       = {0.11f, 0.11f, 0.14f, 1.0f};  // #1c1c24
    ImVec4 bg_sidebar    = {0.09f, 0.09f, 0.12f, 1.0f};  // #17171e
    ImVec4 bg_input      = {0.13f, 0.13f, 0.17f, 1.0f};  // #21212b
    ImVec4 bg_status     = {0.07f, 0.07f, 0.10f, 1.0f};  // #121219
    ImVec4 bg_selected   = {0.20f, 0.20f, 0.28f, 1.0f};  // message highlight

    // Text
    ImVec4 text_default  = {0.80f, 0.80f, 0.80f, 1.0f};  // #cccccc
    ImVec4 text_dim      = {0.50f, 0.50f, 0.55f, 1.0f};  // timestamps, system msgs
    ImVec4 text_bright   = {1.00f, 1.00f, 1.00f, 1.0f};  // own messages

    // Nick colors (cycle through for different users, WeeChat-style)
    std::array<ImVec4, 16> nick_colors = {{
        {0.47f, 0.78f, 1.00f, 1.0f},  // cyan
        {0.53f, 1.00f, 0.53f, 1.0f},  // green
        {1.00f, 0.60f, 0.60f, 1.0f},  // red
        {1.00f, 0.85f, 0.40f, 1.0f},  // yellow
        {0.78f, 0.58f, 1.00f, 1.0f},  // purple
        {1.00f, 0.65f, 0.30f, 1.0f},  // orange
        {0.40f, 1.00f, 0.85f, 1.0f},  // teal
        {1.00f, 0.50f, 0.80f, 1.0f},  // pink
        {0.60f, 0.85f, 1.00f, 1.0f},  // light blue
        {0.85f, 1.00f, 0.60f, 1.0f},  // lime
        {1.00f, 0.75f, 0.75f, 1.0f},  // salmon
        {0.75f, 0.75f, 1.00f, 1.0f},  // lavender
        {1.00f, 0.90f, 0.60f, 1.0f},  // gold
        {0.60f, 1.00f, 0.75f, 1.0f},  // mint
        {1.00f, 0.60f, 1.00f, 1.0f},  // magenta
        {0.70f, 0.90f, 0.90f, 1.0f},  // pale cyan
    }};

    // UI accents
    ImVec4 unread_indicator = {1.00f, 1.00f, 1.00f, 1.0f};  // white bullet
    ImVec4 mention_badge    = {0.90f, 0.20f, 0.20f, 1.0f};  // red mention count
    ImVec4 separator_line   = {0.25f, 0.25f, 0.30f, 1.0f};  // "new messages" line
    ImVec4 url_color        = {0.40f, 0.60f, 1.00f, 1.0f};  // clickable links
    ImVec4 code_bg          = {0.08f, 0.08f, 0.11f, 1.0f};  // inline code/code block bg

    // Reaction badges
    ImVec4 reaction_bg        = {0.18f, 0.18f, 0.24f, 1.0f};
    ImVec4 reaction_bg_active = {0.22f, 0.30f, 0.45f, 1.0f};  // user has reacted
    ImVec4 reaction_text      = {0.70f, 0.70f, 0.75f, 1.0f};
    ImVec4 reaction_count     = {0.90f, 0.90f, 0.95f, 1.0f};

    // Compute nick color from user ID (deterministic hash)
    ImVec4 nickColor(const std::string& user_id) const {
        size_t hash = std::hash<std::string>{}(user_id);
        return nick_colors[hash % nick_colors.size()];
    }
};
```

### 8.4 Message Rendering Pipeline

Each message is converted to a series of `RenderLine` objects:

1. **Timestamp** — `HH:MM` in dim color, fixed width column
2. **Nickname** — right-aligned in user's nick color, fixed width column
3. **Text lines** — mrkdwn parsed to styled ImGui text spans
4. **Inline images** — rendered as GL textures between text lines
5. **Attachments** — rendered with colored left border (Slack attachment style)
6. **Reactions** — rendered as `[:emoji: N]` badges in a horizontal flow

### 8.5 Slack mrkdwn Parser

Slack uses a subset of Markdown called "mrkdwn." The parser converts raw text to a list of styled spans.

**Supported syntax:**

| Syntax | Rendering |
|--------|-----------|
| `*bold*` | Bold text |
| `_italic_` | Italic text |
| `~strike~` | Strikethrough |
| `` `code` `` | Inline code (monospace on code_bg) |
| ` ```code block``` ` | Multi-line code block |
| `>quote` | Block quote with left border |
| `<URL\|label>` | Clickable link |
| `<@U1234>` | @mention → resolve to display name |
| `<#C1234\|channel-name>` | #channel reference → clickable |
| `<!here>` `<!channel>` `<!everyone>` | @here/@channel/@everyone highlight |
| `:emoji_name:` | Emoji rendering |
| Newlines | Preserved as-is |

**Parser output:**

```cpp
struct TextSpan {
    enum class Style { Normal, Bold, Italic, Strike, Code, Link, Mention, ChannelRef, Emoji, Quote };
    Style style;
    std::string text;        // display text
    std::string link_url;    // for Style::Link
    std::string reference;   // user_id for Mention, channel_id for ChannelRef, emoji_name for Emoji
    ImVec4 color;            // computed at parse time
};

// Parse "Hey *bold* and _italic_ with :thumbsup:" into:
// [Normal:"Hey "], [Bold:"bold"], [Normal:" and "], [Italic:"italic"], [Normal:" with "], [Emoji:"thumbsup"]
std::vector<TextSpan> parseMrkdwn(const std::string& text, const UserCache& users, const ChannelCache& channels);
```

---

## 9. Media Pipeline

### 9.1 Image Loading

```
URL → ThreadPool::download() → FileCache::store() → stb_image decode → glTexImage2D → GLuint texture_id
```

**Steps:**
1. When a message with files/images enters the render pipeline, check `FileCache` for the URL.
2. If cached on disk, decode in thread pool and upload texture on main thread.
3. If not cached, download via `WebAPI::downloadFile()` (adds auth header for `url_private`), store to disk cache, then decode + upload.
4. While loading, render a placeholder: `[loading: filename.png ...]`
5. Images are scaled to fit within `image_max_width × image_max_height` while preserving aspect ratio.
6. Texture is stored in a `TextureCache` (LRU, max ~200 textures to avoid GPU memory exhaustion).
7. When a texture is evicted from LRU, call `glDeleteTextures`. The disk cache persists.

### 9.2 Animated GIF

```
URL → download → giflib decode all frames → frame textures[] → animate on timer
```

**Implementation:**
```cpp
struct AnimatedGif {
    std::vector<GLuint> frame_textures;
    std::vector<int> frame_delays_ms;     // per-frame delay
    int current_frame = 0;
    float elapsed_ms = 0.0f;
    int width, height;

    GLuint currentTexture() const { return frame_textures[current_frame]; }

    void update(float delta_ms) {
        elapsed_ms += delta_ms;
        while (elapsed_ms >= frame_delays_ms[current_frame]) {
            elapsed_ms -= frame_delays_ms[current_frame];
            current_frame = (current_frame + 1) % frame_textures.size();
        }
    }
};
```

- GIFs are decoded on a worker thread. Each frame is converted to RGBA.
- Frame textures are uploaded to GPU on the main thread (batch upload, not all at once — upload 1-2 frames per render loop to avoid stutter).
- Only GIFs currently visible in the viewport are animated (check scroll position).
- GIFs beyond viewport or in background buffers pause animation and may have frames evicted.

### 9.3 Emoji Rendering

Two emoji sources:
1. **Standard Unicode emoji** — rendered from a pre-built spritesheet atlas (e.g., Twemoji or Apple emoji set). Each emoji maps to a UV rect on the atlas texture.
2. **Custom Slack workspace emoji** — fetched via `emoji.list` API. URLs are downloaded and cached like images, but at emoji size (16-20px).

```cpp
class EmojiRenderer {
public:
    void loadAtlas(const std::string& atlas_path);
    void loadCustomEmoji(const std::map<std::string, std::string>& name_to_url);

    // Returns true if emoji exists, renders inline at cursor position
    bool renderInline(const std::string& emoji_name, float size);

    // Get UV rect for atlas emoji
    bool getAtlasUV(const std::string& emoji_name, ImVec2& uv0, ImVec2& uv1);

private:
    GLuint atlas_texture_;
    std::unordered_map<std::string, ImVec4> atlas_uvs_;  // name -> (u0,v0,u1,v1)
    std::unordered_map<std::string, GLuint> custom_textures_;  // name -> texture
};
```

### 9.4 File Cache

```cpp
class FileCache {
public:
    FileCache(const std::string& cache_dir, size_t max_size_bytes = 500 * 1024 * 1024);

    // Store downloaded file, returns local path
    std::string store(const std::string& url, const std::vector<uint8_t>& data);

    // Get local path if cached, nullopt if not
    std::optional<std::string> get(const std::string& url);

    // Evict oldest files when cache exceeds max_size_bytes
    void prune();

private:
    std::string cache_dir_;
    size_t max_size_;
    // Track file access times for LRU eviction
    std::map<std::string, std::filesystem::file_time_type> access_times_;
};
```

Cache location: `~/.config/conduit/cache/<team_id>/`

---

## 10. SQLite Schema

One database file per org: `~/.config/conduit/data/<team_id>.db`

```sql
-- Schema version tracking
CREATE TABLE schema_version (
    version INTEGER NOT NULL
);
INSERT INTO schema_version VALUES (1);

-- Users
CREATE TABLE users (
    id TEXT PRIMARY KEY,          -- U-prefixed
    display_name TEXT NOT NULL,
    real_name TEXT DEFAULT '',
    avatar_url_72 TEXT DEFAULT '',
    avatar_url_192 TEXT DEFAULT '',
    status_emoji TEXT DEFAULT '',
    status_text TEXT DEFAULT '',
    is_bot INTEGER DEFAULT 0,
    updated_at INTEGER DEFAULT (strftime('%s', 'now'))
);

-- Channels
CREATE TABLE channels (
    id TEXT PRIMARY KEY,
    name TEXT NOT NULL,
    topic TEXT DEFAULT '',
    purpose TEXT DEFAULT '',
    type TEXT NOT NULL,          -- 'public', 'private', 'dm', 'mpdm', 'group'
    is_member INTEGER DEFAULT 0,
    is_muted INTEGER DEFAULT 0,
    is_archived INTEGER DEFAULT 0,
    member_count INTEGER DEFAULT 0,
    dm_user_id TEXT DEFAULT '',  -- for DMs
    last_read TEXT DEFAULT '',   -- timestamp
    updated_at INTEGER DEFAULT (strftime('%s', 'now'))
);

-- Messages
CREATE TABLE messages (
    channel_id TEXT NOT NULL,
    ts TEXT NOT NULL,             -- Slack timestamp (serves as unique ID within channel)
    thread_ts TEXT DEFAULT '',
    user_id TEXT NOT NULL,
    text TEXT NOT NULL,
    subtype TEXT DEFAULT '',
    is_edited INTEGER DEFAULT 0,
    edited_ts TEXT DEFAULT '',
    reply_count INTEGER DEFAULT 0,
    is_pinned INTEGER DEFAULT 0,
    reactions_json TEXT DEFAULT '[]',      -- JSON array of Reaction objects
    attachments_json TEXT DEFAULT '[]',    -- JSON array of Attachment objects
    files_json TEXT DEFAULT '[]',          -- JSON array of SlackFile objects
    reply_users_json TEXT DEFAULT '[]',    -- JSON array of user IDs
    PRIMARY KEY (channel_id, ts)
);

CREATE INDEX idx_messages_channel_ts ON messages(channel_id, ts DESC);
CREATE INDEX idx_messages_thread ON messages(channel_id, thread_ts) WHERE thread_ts != '';
CREATE INDEX idx_messages_user ON messages(user_id);

-- Pins
CREATE TABLE pins (
    channel_id TEXT NOT NULL,
    ts TEXT NOT NULL,
    pinned_by TEXT NOT NULL,
    pinned_at INTEGER NOT NULL,
    PRIMARY KEY (channel_id, ts)
);

-- Custom emoji
CREATE TABLE custom_emoji (
    name TEXT PRIMARY KEY,
    url TEXT NOT NULL,
    is_alias INTEGER DEFAULT 0,
    alias_for TEXT DEFAULT ''
);

-- Read state (last read position per channel)
CREATE TABLE read_state (
    channel_id TEXT PRIMARY KEY,
    last_read_ts TEXT NOT NULL,
    unread_count INTEGER DEFAULT 0,
    mention_count INTEGER DEFAULT 0
);

-- File cache metadata
CREATE TABLE file_cache (
    url TEXT PRIMARY KEY,
    local_path TEXT NOT NULL,
    size_bytes INTEGER NOT NULL,
    downloaded_at INTEGER DEFAULT (strftime('%s', 'now')),
    last_accessed INTEGER DEFAULT (strftime('%s', 'now'))
);

-- Input history per channel
CREATE TABLE input_history (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    channel_id TEXT NOT NULL,
    text TEXT NOT NULL,
    entered_at INTEGER DEFAULT (strftime('%s', 'now'))
);

CREATE INDEX idx_input_history_channel ON input_history(channel_id, entered_at DESC);
```

---

## 11. Keybindings & Commands

### 11.1 Global Keybindings

| Key | Action |
|-----|--------|
| `Alt+1..9` | Switch to buffer 1-9 |
| `Alt+0` | Switch to buffer 10 |
| `Alt+J` then number | Jump to buffer by two-digit number |
| `Alt+A` | Jump to next buffer with unread activity |
| `Alt+←` / `Alt+→` | Switch to previous/next buffer |
| `Alt+↑` / `Alt+↓` | Switch to previous/next buffer with unread |
| `Ctrl+K` | Open command palette (fuzzy channel/command search) |
| `Ctrl+F` | Open search panel |
| `Ctrl+L` | Clear buffer display (not messages) |
| `F5` / `F6` | Toggle buffer list sidebar |
| `F7` / `F8` | Toggle nick list sidebar |
| `PgUp` / `PgDn` | Scroll buffer |
| `Home` | Scroll to top (loads history) |
| `End` | Scroll to bottom |
| `Escape` | Close overlay/panel, deselect message, or clear input |
| `Alt+N` | Open org switcher |
| `Alt+T` | Open thread panel for selected message |
| `Alt+R` | Open reaction picker for selected message |
| `Alt+E` | Edit selected message (if own) |
| `Alt+D` | Delete selected message (if own, with confirm) |
| `Ctrl+U` | Upload file to current channel |

### 11.2 Input Bar Keybindings

| Key | Action |
|-----|--------|
| `Enter` | Send message |
| `Shift+Enter` | Insert newline (multi-line input) |
| `↑` / `↓` | Browse input history (when input empty) |
| `Tab` | Autocomplete @user, #channel, :emoji:, /command |
| `Shift+Tab` | Reverse autocomplete cycle |
| `Ctrl+A` | Move cursor to start |
| `Ctrl+E` | Move cursor to end |
| `Ctrl+W` | Delete word backward |
| `Ctrl+K` | Delete to end of line |
| `Ctrl+U` | Delete entire line |

### 11.3 Slash Commands

```
/join #channel          — Join a channel
/leave                  — Leave current channel
/part                   — Alias for /leave
/msg @user message      — Send direct message
/query @user            — Open DM buffer with user
/topic new topic text   — Set channel topic
/me action text         — Send action message
/react :emoji:          — Add reaction to last message (or selected)
/unreact :emoji:        — Remove reaction
/pin                    — Pin selected message
/unpin                  — Unpin selected message
/search query           — Search messages
/upload filepath        — Upload a file
/status :emoji: text    — Set your Slack status
/away                   — Toggle away status
/clear                  — Clear buffer display
/close                  — Close current buffer
/reconnect              — Force reconnect Socket Mode
/debug                  — Toggle debug log buffer
/set key=value          — Change runtime config
/help                   — Show command help
/org list               — List connected orgs
/org switch name        — Switch to org by name
/org connect            — Add new org
/org disconnect name    — Remove org
/thread                 — Open thread panel for selected message
/reply text             — Reply in thread of selected message
/edit new text          — Edit selected message (must be own)
/delete                 — Delete selected message (with confirmation)
```

---

## 12. Configuration System

Config file: `~/.config/conduit/conduit.toml`

```toml
[general]
# Default font size in pixels
font_size = 14.0
# Mono font path (relative to assets/ or absolute)
font = "fonts/JetBrainsMono-Regular.ttf"
font_bold = "fonts/JetBrainsMono-Bold.ttf"
font_italic = "fonts/JetBrainsMono-Italic.ttf"
# Theme file
theme = "themes/weechat_dark.toml"
# Log level: trace, debug, info, warn, error
log_level = "info"
# Log file path
log_file = "~/.config/conduit/conduit.log"

[ui]
# Show buffer list sidebar
show_buffer_list = true
# Show nick list sidebar
show_nick_list = true
# Buffer list width
buffer_list_width = 180
# Nick list width
nick_list_width = 160
# Max inline image width
image_max_width = 360
# Max inline image height
image_max_height = 240
# Timestamp format (strftime)
timestamp_format = "%H:%M"
# Show seconds in timestamps
show_seconds = false
# Nick alignment (right, left, none)
nick_alignment = "right"
# Nick column width (chars)
nick_max_width = 15
# Show user avatars (tiny, next to nick)
show_avatars = false
# Animate GIFs (set false to show static first frame)
animate_gifs = true
# Show images inline (false = show as [file: name] links only)
show_inline_images = true
# Buffer sort order: "alphabetical", "activity", "custom"
buffer_sort = "activity"

[notifications]
# Enable desktop notifications
enabled = true
# Notify on all messages or only mentions
notify_on = "mentions"  # "all", "mentions", "dms", "none"
# Notification sound
sound_enabled = false
# Do not disturb hours (24h format)
dnd_start = ""  # e.g. "22:00"
dnd_end = ""    # e.g. "08:00"

[network]
# Connection timeout (seconds)
connect_timeout = 10
# Request timeout (seconds)
request_timeout = 30
# Max reconnect backoff (seconds)
max_reconnect_backoff = 30
# Proxy (empty = no proxy)
proxy = ""
# HTTP proxy for curl
http_proxy = ""

[cache]
# Max disk cache size for files (MB)
max_file_cache_mb = 500
# Max messages to keep per channel in SQLite
max_messages_per_channel = 10000
# Prune messages older than N days (0 = never)
prune_days = 0

[keybindings]
# Override default keybindings (format: "action = key_combo")
# See KEYBINDINGS.md for available actions
# Example:
# switch_next_buffer = "Ctrl+Tab"
# switch_prev_buffer = "Ctrl+Shift+Tab"

# Org-specific configuration
[[org]]
name = "MyCompany"
# App-level token for Socket Mode
app_token = ""  # Will prompt on first run if empty, stored in OS keychain
# User token
user_token = ""  # Same — stored in OS keychain after first entry
# Bot token (optional, for Socket Mode)
bot_token = ""
# Auto-connect on startup
auto_connect = true
# Channels to auto-open as buffers
auto_open = ["#general", "#random"]

[[org]]
name = "SideProject"
app_token = ""
user_token = ""
auto_connect = false
auto_open = []
```

---

## 13. Multi-Org Support

### Architecture

Each org is a fully independent `SlackClient` instance:

```cpp
class OrgManager {
public:
    // Add/remove orgs
    void addOrg(const OrgConfig& config);
    void removeOrg(const std::string& name);

    // Connect/disconnect
    void connectAll();      // auto_connect=true orgs
    void connectOrg(const std::string& name);
    void disconnectOrg(const std::string& name);

    // Switch active org (changes which buffers are visible)
    void switchOrg(const std::string& name);
    void switchOrg(int index);          // Alt+1..9
    void nextOrg();
    void prevOrg();

    // Access
    SlackClient* activeClient();
    SlackClient* client(const std::string& name);
    const std::vector<OrgConfig>& orgs() const;
    int activeOrgIndex() const;

    // Aggregate unread count across all orgs
    int totalUnreadCount() const;
    int totalMentionCount() const;

    // Process events from all connected orgs
    void pollAllEvents();

private:
    struct OrgInstance {
        OrgConfig config;
        std::unique_ptr<SlackClient> client;
        std::unique_ptr<Database> db;
        bool is_connected = false;
    };

    std::vector<OrgInstance> orgs_;
    int active_index_ = 0;
};
```

### Buffer Naming Convention

Buffers are prefixed with the org name when multiple orgs are connected:

```
Single org:        #general, #random, @alice
Multiple orgs:     MyCompany/#general, SideProject/#general
```

The `BufferList` renders org headers as separators:

```
 MyCompany         ← org header (clickable to switch)
  #general
  #random
  @alice
 SideProject       ← org header
  #dev
  @bob
```

### Token Storage

Tokens are NEVER stored in the TOML config file in production. On first run (or if tokens are empty), Conduit prompts for them via the input bar and stores them in the OS keychain:

| Platform | Storage |
|----------|---------|
| Windows | Windows Credential Manager (DPAPI via `CredWrite/CredRead`) |
| Linux | libsecret (GNOME Keyring / KWallet) |
| macOS | Keychain Services (`SecItemAdd/SecItemCopyMatching`) |

Keychain entries:
```
Service: "conduit"
Account: "<org_name>/app_token"
Account: "<org_name>/user_token"
```

Fallback if no keychain available: encrypted file `~/.config/conduit/tokens.enc` with a user-provided passphrase.

---

## 14. Security Considerations

1. **Token storage:** Keychain-backed, never plaintext on disk. Config file stores org name and metadata only.
2. **File downloads:** All `url_private` URLs require the user token in the `Authorization` header. Downloaded files go to the sandboxed cache directory only.
3. **URL handling:** External URLs opened via `xdg-open`/`open`/`start` — never fetched and rendered as HTML. This is a text client.
4. **Input sanitization:** Slash commands are parsed locally. User text is sent as-is to Slack's API (Slack handles server-side sanitization).
5. **WebSocket security:** Socket Mode URLs are `wss://` (TLS). Verify SSL certificates.
6. **Memory:** Tokens in memory are `std::string` — consider `SecureString` wrapper that zeroes on destruction for the truly paranoid.
7. **Logging:** Logger MUST redact tokens from any output. Pattern: replace `xoxp-`, `xoxb-`, `xapp-` prefixed strings with `xo**-REDACTED`.

---

## 15. Phased Build Plan

### Phase 0: Skeleton (Week 1)

**Goal:** Window opens, ImGui renders, monospace font loaded, WeeChat layout visible with hardcoded placeholder text.

- [ ] CMakeLists.txt with all dependencies fetching/linking
- [ ] `main.cpp`: SDL2 init, OpenGL context, ImGui init with docking enabled
- [ ] Load JetBrains Mono font
- [ ] `Theme.h/.cpp`: Apply WeeChat dark color scheme to ImGui style
- [ ] `UIManager.h/.cpp`: Render the 5-panel layout (title, sidebar, buffer, nicklist, input+status)
- [ ] Placeholder text in each panel to validate layout proportions
- [ ] F5/F6/F7/F8 toggles for sidebars
- [ ] Window resize handling

**Deliverable:** A window that looks like WeeChat with fake data.

### Phase 1: Slack Connection (Week 2)

**Goal:** Connect to one Slack org, fetch channels and users, display in sidebar.

- [ ] `Config.h/.cpp`: TOML parser
- [ ] `KeychainStore.h/.cpp`: Platform token storage (start with Windows DPAPI/Credential Manager)
- [ ] `WebAPI.h/.cpp`: libcurl wrapper, auth header injection, JSON response parsing
- [ ] `SlackAuth.h/.cpp`: `auth.test` validation
- [ ] `SocketModeClient.h/.cpp`: WebSocket connect, hello handshake, envelope ack
- [ ] `RateLimiter.h/.cpp`: Per-method rate tracking
- [ ] `Database.h/.cpp`: SQLite wrapper, schema creation, migrations
- [ ] `ChannelCache.h/.cpp`: Fetch and cache `conversations.list`
- [ ] `UserCache.h/.cpp`: Fetch and cache `users.list`
- [ ] `BufferList.h/.cpp`: Render real channel list from cache
- [ ] `ThreadPool.h/.cpp`: Basic async task execution

**Deliverable:** Sidebar shows real channels from a connected Slack workspace.

### Phase 2: Messages (Week 3-4)

**Goal:** Load and display message history, send messages, receive real-time messages.

- [ ] `MessageCache.h/.cpp`: Store/retrieve messages from SQLite
- [ ] `SlackClient.h/.cpp`: `conversations.history`, `chat.postMessage`
- [ ] `EventDispatcher.h/.cpp`: Route Socket Mode events to handlers
- [ ] `Types.h`: All Slack data structs
- [ ] `BufferView.h/.cpp`: Render messages with timestamp + nick + text columns
- [ ] `TextRenderer.h/.cpp`: Basic mrkdwn parser (bold, italic, code, links)
- [ ] `InputBar.h/.cpp`: Text input with Enter to send
- [ ] `InputHistory.h/.cpp`: Up/down input history
- [ ] `StatusBar.h/.cpp`: Connection state, org name, clock
- [ ] `TitleBar.h/.cpp`: Channel name + topic
- [ ] Real-time message delivery via Socket Mode
- [ ] Auto-scroll to bottom on new messages
- [ ] Scroll up to load older messages
- [ ] Mark-as-read on channel switch (`conversations.mark`)
- [ ] Unread indicators in buffer list

**Deliverable:** Full text chat working — send and receive messages in real time.

### Phase 3: Rich Content (Week 5-6)

**Goal:** Images, GIFs, reactions, emoji, code blocks, threads.

- [ ] `ImageRenderer.h/.cpp`: Download, decode, texture upload, inline rendering
- [ ] `GifRenderer.h/.cpp`: Animated GIF frame management
- [ ] `FileCache.h/.cpp`: Disk-backed media cache with LRU eviction
- [ ] `EmojiRenderer.h/.cpp`: Emoji atlas + custom emoji
- [ ] `ReactionBadge.h/.cpp`: Render `[:emoji: N]` badges
- [ ] Reaction add/remove via `Alt+R` picker or `/react` command
- [ ] `CodeBlock.h/.cpp`: Syntax-highlighted code blocks (simple keyword highlight, not full grammar)
- [ ] `ThreadPanel.h/.cpp`: Side panel for thread replies
- [ ] Thread reply via `/reply` or `Alt+T`
- [ ] `LinkHandler.h/.cpp`: Clickable URLs (open in browser), @mentions, #channels
- [ ] `NickList.h/.cpp`: Real member list with presence indicators
- [ ] Presence change events
- [ ] Typing indicators in status bar
- [ ] File upload via `/upload` or `Ctrl+U`

**Deliverable:** Full rich media experience.

### Phase 4: Power Features (Week 7-8)

**Goal:** Search, slash commands, completions, multi-org, notifications.

- [ ] `CommandParser.h/.cpp`: Full slash command routing
- [ ] `TabComplete.h/.cpp`: @user, #channel, :emoji:, /command completion
- [ ] `SearchPanel.h/.cpp`: `Ctrl+F` search overlay using `search.messages` API
- [ ] `CommandPalette.h/.cpp`: `Ctrl+K` fuzzy finder
- [ ] `OrgSwitcher.h/.cpp`: `Alt+N` org switch overlay
- [ ] `OrgManager`: Full multi-org lifecycle
- [ ] `NotificationManager.h/.cpp`: Desktop notifications via `libnotify` (Linux), `NSUserNotification` (mac), `WinToast` (Windows)
- [ ] Pin/unpin messages
- [ ] Message edit/delete
- [ ] User status set/clear
- [ ] `FilePreview.h/.cpp`: Full-size image/file preview modal
- [ ] `Logger.h/.cpp`: Debug log buffer accessible via `/debug`

**Deliverable:** Feature-complete Slack replacement.

### Phase 5: Polish (Week 9-10)

- [ ] Window title shows unread count: `Conduit [3]`
- [ ] System tray icon with unread badge (platform-specific)
- [ ] Proper exit handling (graceful WebSocket close, flush SQLite)
- [ ] Config hot-reload for theme changes
- [ ] Performance profiling: target 60fps with 10k cached messages
- [ ] Memory profiling: cap texture memory, tune LRU sizes
- [ ] Cross-platform build CI (GitHub Actions: Linux, macOS, Windows)
- [ ] Installer/package: AppImage (Linux), .dmg (macOS), .msi (Windows)
- [ ] README with screenshots, setup guide, Slack App creation instructions
- [ ] `KEYBINDINGS.md` and `SLASH_COMMANDS.md` documentation

---

## 16. Testing Strategy

### Unit Tests

| Test File | Coverage |
|-----------|----------|
| `test_mrkdwn_parser.cpp` | All mrkdwn syntax → TextSpan conversion |
| `test_command_parser.cpp` | Slash command parsing, argument extraction |
| `test_cache.cpp` | SQLite CRUD for messages, channels, users |
| `test_rate_limiter.cpp` | Bucket tracking, wait logic, 429 handling |
| `test_types.cpp` | JSON ↔ struct serialization roundtrip |
| `test_config.cpp` | TOML parsing, defaults, invalid input handling |
| `test_time_format.cpp` | Relative/absolute timestamp formatting |
| `test_tab_complete.cpp` | Completion candidate ranking and cycling |

### Integration Tests

- **Mock Slack server:** A simple HTTP/WebSocket server that replays canned responses for API calls and Socket Mode events. Used to test full `SlackClient` flow without hitting real Slack.
- **Render regression:** Screenshot comparison for BufferView rendering with known message sets.

### Manual Test Checklist

- [ ] Connect to real Slack org
- [ ] Send/receive messages in real time
- [ ] Images render inline
- [ ] GIFs animate
- [ ] Reactions display and can be added/removed
- [ ] Thread panel opens and shows replies
- [ ] Search returns results
- [ ] File upload works
- [ ] Slash commands work
- [ ] Tab completion works for @users, #channels, :emoji:
- [ ] Multi-org switching works
- [ ] Desktop notifications fire on mentions
- [ ] 1000+ message buffer scrolls smoothly at 60fps
- [ ] Reconnects after network drop

---

## 17. Known Challenges & Mitigations

### Challenge 1: ImGui Text Layout

**Problem:** ImGui's text rendering is simple — it doesn't natively support rich text spans, word wrapping with mixed styles, or inline images between text lines.

**Mitigation:** Build a custom `TextRenderer` that:
- Pre-calculates line breaks using `ImGui::CalcTextSize()` per word
- Renders each span with `ImGui::PushStyleColor` / `ImGui::SetCursorPosX` / `ImGui::SameLine`
- Uses `ImGui::Image()` between text lines for inline media
- Caches computed layout per message (invalidate on resize or content change)

### Challenge 2: Texture Memory

**Problem:** Large Slack workspaces may have hundreds of images in scroll history.

**Mitigation:** LRU texture cache (max ~200 textures). Textures scroll out of viewport are kept in LRU; once evicted, they're re-decoded from disk cache on next scroll-into-view. Profile and tune the number.

### Challenge 3: Socket Mode Reliability

**Problem:** Socket Mode connections can drop silently.

**Mitigation:**
- Heartbeat/ping on the WebSocket every 30s
- If no pong received within 10s, force reconnect
- Exponential backoff with jitter on reconnect
- On reconnect, fetch missed messages via `conversations.history` with `oldest=last_received_ts` for each open channel

### Challenge 4: Slack API Pagination

**Problem:** `conversations.list`, `users.list`, and history endpoints are paginated with cursors.

**Mitigation:** Generic pagination helper:
```cpp
template<typename T>
std::vector<T> paginateAll(
    std::function<nlohmann::json(const std::string& cursor)> fetcher,
    std::function<std::vector<T>(const nlohmann::json&)> extractor
);
```

### Challenge 5: Custom Emoji

**Problem:** Workspaces can have thousands of custom emoji, some aliased.

**Mitigation:** Fetch `emoji.list` on connect, store in SQLite. Resolve aliases at parse time. Download emoji images lazily (on first render, not on connect). Cache as small textures (20x20px).

### Challenge 6: Cross-Platform Keychain

**Problem:** Three different keychain APIs across platforms.

**Mitigation:** Abstract behind `KeychainStore` interface. Implement platform-specific backends behind `#ifdef`. Fallback to AES-256-encrypted file if no keychain available (prompt for passphrase).

---

## Appendix A: Slack App Setup Instructions (for README)

1. Go to https://api.slack.com/apps
2. Click "Create New App" → "From scratch"
3. Name it "Conduit" and select your workspace
4. Under "OAuth & Permissions", add the user token scopes listed in Section 7.1
5. Install the app to your workspace → copy the `xoxp-` User OAuth Token
6. Under "Socket Mode", enable it → generate an App-Level Token with `connections:write` → copy the `xapp-` token
7. Under "Event Subscriptions", enable events and subscribe to the events listed in Section 7.2
8. Run Conduit, it will prompt you to enter both tokens on first launch

---

## Appendix B: Reference Projects

- [WeeChat](https://github.com/weechat/weechat) — UI layout reference, keybinding conventions
- [wee-slack](https://github.com/wee-slack/wee-slack) — WeeChat Slack plugin, API usage patterns
- [slack-term](https://github.com/erroneousboat/slack-term) — Go-based terminal Slack client
- [Dear ImGui](https://github.com/ocornut/imgui) — UI framework docs and examples
- [Slack API docs](https://api.slack.com/methods) — Method reference
- [Slack Socket Mode](https://api.slack.com/apis/socket-mode) — Real-time connection docs
- [Slack Events API](https://api.slack.com/events) — Event type reference

---

## Appendix C: Future Considerations (Post-MVP)

- **Huddle/Call support:** Slack huddles use WebRTC — significant undertaking, defer.
- **Workflow Builder integration:** Respond to workflow step events.
- **Slack Connect:** Cross-org shared channels — requires additional OAuth scopes.
- **Plugin system:** Lua or Python scripting for custom commands (WeeChat has this).
- **Vim mode:** Vi-style keybindings in input bar and buffer navigation.
- **Split view:** Multiple buffers visible side-by-side (ImGui docking makes this natural).
- **Message bookmarks:** Local-only message bookmarking system.
- **Export:** Export channel history to markdown/HTML.
