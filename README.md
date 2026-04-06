# Conduit

**A Slack client for people who think 1.2GB of RAM for a chat app is a war crime.**

Conduit is a WeeChat-style Slack client built in C++ with Dear ImGui. It looks like you're SSH'd into a server from 2003. It renders inline images and animated GIFs. It connects via the same WebSocket the browser uses. It uses about 40MB of RAM. The Slack desktop client is currently crying in the corner.

## Why does this exist?

Because Slack's official client is an Electron app. For the uninitiated, that means they shipped an entire copy of Google Chrome just so you can read "sounds good, let's circle back on that." Every. Single. Day.

Conduit exists because:
- **Electron ate my RAM** and I want it back. All of it. Every last byte.
- **WeeChat keybindings** are objectively superior to clicking around with a mouse like some kind of animal.
- **Terminal aesthetics** are not a phase, mom. Pure black background, monospace font, colored nicks. This is who I am.
- **I shouldn't need admin privileges** just to use a chat client. Conduit works with any Slack workspace. No app installation. No OAuth dance. No begging IT for permissions.

## What it does

- **Real-time messaging** via WebSocket (same connection the browser uses, not polling like a caveman)
- **Inline images and animated GIFs** because we're not *completely* unhinged
- **Full-color emoji** pulled straight from Slack's CDN — custom workspace emoji, animated GIFs, the works
- **Unicode emoji rendering** via system fonts as fallback
- **IRC-style messages** — `20:34 <alice> hey everyone` — because this is how chat should look
- **Click-to-view images** in a full-screen overlay, click-to-toggle reactions
- **Threads** with dedicated side panel and reply input — click thread counts to open
- **Hover action bar** on messages — React and Reply buttons appear on hover like the real client
- **Structured sidebar** — collapsible Channels, Direct Messages, and Apps sections
- **DMs that actually work** — open DMs from nick list, `/msg`, `/query`, self-DM, bot conversations
- **@mention autocomplete** — Tab completion and live popup as you type
- **Real @mentions** — converts `@username` to Slack's `<@UID>` format so people actually get notified
- **Search, file upload, clipboard paste** (Ctrl+V an image, it just works)
- **Right-click context menus** with quick reaction emoji row
- **Typing indicators, presence dots, unread badges** — all the stuff you'd expect
- **Keyboard-driven everything** — Alt+1-9, /slash commands, tab completion, Ctrl+K command palette
- **Draggable resizable panes** — sidebar widths adjust and persist between sessions
- **Zero-config authentication** — steals your token from Chrome/Edge automatically, or paste it manually
- **DPI-aware with Ctrl+/- scaling** — works on your 4K monitor and your potato laptop

## What it doesn't do

- **Use Electron.** You're welcome. Your laptop fan is welcome. Your battery is welcome.
- **Look like Slack.** That's not a bug, that's the entire point.
- **Phone home.** No telemetry, no analytics, no "we'd love your feedback" popups.
- **Require admin privileges.** Works with any workspace you can log into.
- **Crash.** (Often.) (Usually.) (Look, it's C++, things happen.)

## Screenshots

It's a black terminal with colored text. Use your imagination. Or just run it.

## Getting Started

### The Zero-Effort Way (recommended)

1. Log into Slack in Chrome, Edge, Brave, or any Chromium browser
2. Build and run Conduit
3. There is no step 3

Conduit automatically finds your Slack credentials from the browser's local storage and cookies. It reads the `xoxc-` token from Chrome's LevelDB (unencrypted — thanks, Google) and decrypts the `d` session cookie using Windows DPAPI. Same user, same machine, zero friction. Your token is saved to Windows Credential Manager so this only happens once.

*"Isn't that basically stealing credentials?"* Yes. From yourself. You're welcome.

### The Manual Way (if auto-detect fails)

If you don't have Slack open in a browser, or you're on macOS/Linux where cookie decryption isn't implemented yet:

1. Open Slack in your browser (app.slack.com)
2. Press F12 → Console tab
3. Paste this:
   ```js
   JSON.parse(localStorage.localConfig_v2).teams[Object.keys(JSON.parse(localStorage.localConfig_v2).teams)[0]].token
   ```
4. Copy the `xoxc-...` token, paste it into Conduit, hit Enter
5. Now it asks for the `d` cookie:
   - F12 → Application tab → Cookies → `.slack.com` → find `d` → copy the value
6. Paste it, hit Enter
7. Done forever. Token saved to OS credential store.

### The Config File Way (for automation nerds)

Put your tokens in `%APPDATA%\conduit\conduit.toml`:

```toml
[[org]]
name = "MyWorkspace"
user_token = "xoxp-your-token-here"
app_token = "xapp-your-app-token-here"
auto_connect = true
```

## Building

### Windows

#### Prerequisites

You need three things. If you already have them, skip ahead. If you don't, welcome to C++ development. It builds character.

1. **Visual Studio 2022** (Community Edition is free)
   - Download from https://visualstudio.microsoft.com/
   - During install, select **"Desktop development with C++"** workload
   - This gives you MSVC compiler, Windows SDK, CMake, and the will to live (temporarily)

2. **CMake 3.24+** (usually comes with VS, but just in case)
   - Download from https://cmake.org/download/
   - Or: `winget install Kitware.CMake`
   - Make sure it's in your PATH: `cmake --version` should work

3. **vcpkg** (C++ package manager — think npm but for people who suffer)
   ```cmd
   git clone https://github.com/microsoft/vcpkg.git C:\vcpkg
   C:\vcpkg\bootstrap-vcpkg.bat
   ```
   Add `C:\vcpkg` to your PATH, or just use the full path everywhere.

#### Build Steps

```cmd
cd conduit
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Release
```

First build takes a while because vcpkg compiles all dependencies from source. Subsequent builds are fast. The executable lands at `build\Release\conduit.exe`.

#### Run

```cmd
build\Release\conduit.exe
```

That's it. No `npm install`. No `yarn`. No `node_modules` folder the size of a small country.

### macOS

#### Prerequisites

1. **Xcode Command Line Tools** (you probably already have these)
   ```bash
   xcode-select --install
   ```

2. **Homebrew** (if you don't have this, how do you even Mac?)
   ```bash
   /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
   ```

3. **Dependencies**
   ```bash
   brew install cmake ninja sdl2 curl sqlite3 giflib libwebsockets nlohmann-json create-dmg
   ```

#### Build Steps

```bash
cd conduit
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

#### Run

```bash
./build/conduit
```

Note: macOS cookie decryption isn't implemented yet, so you'll need to paste your token manually (see "The Manual Way" above). The auto-detect from Chrome will get the token but not the cookie.

### Linux

#### Prerequisites (Ubuntu/Debian)

```bash
sudo apt update
sudo apt install -y \
  build-essential \
  cmake \
  ninja-build \
  libsdl2-dev \
  libcurl4-openssl-dev \
  libsqlite3-dev \
  libgl-dev \
  libgif-dev \
  libwebsockets-dev \
  nlohmann-json3-dev
```

#### Prerequisites (Fedora/RHEL)

```bash
sudo dnf install -y \
  gcc-c++ \
  cmake \
  ninja-build \
  SDL2-devel \
  libcurl-devel \
  sqlite-devel \
  mesa-libGL-devel \
  giflib-devel \
  libwebsockets-devel \
  json-devel
```

#### Prerequisites (Arch, btw)

```bash
sudo pacman -S --needed \
  base-devel \
  cmake \
  ninja \
  sdl2 \
  curl \
  sqlite \
  giflib \
  libwebsockets \
  nlohmann-json
```

#### Build Steps

```bash
cd conduit
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

#### Run

```bash
./build/conduit
```

Same as macOS — cookie decryption is Windows-only for now. Paste your token manually.

### Common Build Issues

| Problem | Solution |
|---------|----------|
| `cmake` not found | Install it. `winget install Kitware.CMake` on Windows, `brew install cmake` on Mac |
| vcpkg errors on Windows | Make sure `-DCMAKE_TOOLCHAIN_FILE` points to the right path |
| `SDL2 not found` | Install SDL2 dev package for your platform |
| `libwebsockets not found` | Install `libwebsockets-dev` (Ubuntu) or `libwebsockets` (brew/pacman) |
| `GL/gl.h not found` on Mac | This is normal, the build uses `OpenGL/gl.h` on macOS automatically |
| First build takes forever | vcpkg compiles from source. Go get coffee. Second build is fast. |
| `nlohmann_json not found` | Install `nlohmann-json3-dev` (Ubuntu) or `nlohmann-json` (brew/pacman) |

## Keybindings

If you've used WeeChat or irssi, you already know most of these. If you haven't, welcome to the good side.

| Key | What it does |
|-----|-------------|
| `Alt+1..9` | Switch buffers (because tabs are for browsers) |
| `Alt+A` | Jump to next unread |
| `Alt+Left/Right` | Navigate buffers |
| `Alt+Up/Down` | Navigate to next/prev unread buffer |
| `Ctrl+K` | Command palette (fuzzy search channels and commands) |
| `Ctrl+F` | Search messages |
| `Ctrl+=/-/0` | Zoom in/out/reset (just like a browser, but better) |
| `Ctrl+V` | Paste image from clipboard and upload it |
| `F5/F6` | Toggle channel list sidebar |
| `F7/F8` | Toggle user list sidebar |
| `Alt+T` | Open thread panel |
| `Alt+R` | Emoji reaction picker |
| `Alt+E` | Edit your last message |
| `Alt+D` | Delete your last message |
| `Home` | Scroll to top (loads older messages) |
| `End` | Scroll to bottom |
| `Escape` | Close whatever overlay is open |
| `Tab` | Autocomplete @users, #channels, :emoji:, /commands |
| `Up/Down` | Input history (when input is empty) |
| `Enter` | Send message |
| `Ctrl+Enter` | Insert newline |

## Slash Commands

```
/join #channel      Join a channel
/leave              Leave current channel
/msg @user text     Open DM and send a message
/query @user        Open DM with user
/topic new topic    Set channel topic
/me does a thing    Action message
/react :emoji:      React to last message
/unreact :emoji:    Remove reaction
/reply text         Reply in thread
/edit new text      Edit your last message
/delete             Delete your last message
/search query       Search messages
/upload filepath    Upload a file
/status :emoji: txt Set your status
/away               Toggle away
/clear              Clear buffer
/close              Close buffer
/reconnect          Force reconnect
/pin                Pin message
/unpin              Unpin message
/set key=value      Change a setting
/org list           List orgs
/help               Show all commands
```

## Architecture

For the three people who care:

- **C++20** because we have standards (pun intended)
- **Dear ImGui** for the UI (immediate mode, 60fps, ~12K lines of actual code)
- **SDL2** for windowing (not Electron, not Qt, not wxWidgets)
- **OpenGL 3.3** for rendering (your integrated GPU handles this fine)
- **libcurl** for REST API calls
- **libwebsockets** for the real-time WebSocket connection
- **SQLite3** for local message/channel/user caching
- **nlohmann/json** for JSON parsing
- **stb_image/stb_image_write** for image decoding/encoding
- **giflib** for animated GIF frame decoding
- **toml++** for config file parsing
- **Lekton Nerd Font Mono** for the typography

Total dependency footprint: ~15MB. Electron's `node_modules`: lol.

## How Authentication Works

Conduit supports three authentication methods, tried in this order:

1. **Keychain** — checks Windows Credential Manager for a previously saved token
2. **Browser snatch** — scans Chrome, Edge, Brave, Vivaldi, and other Chromium browsers for Slack credentials:
   - Reads the `xoxc-` token from `localStorage` (LevelDB files, unencrypted)
   - Reads the `d` session cookie from the Cookies SQLite database
   - Decrypts the cookie using DPAPI (Windows) since it's the same user account
   - Saves both to the OS credential store for next time
3. **Manual paste** — asks you to paste the token and cookie from the browser dev tools

The browser scanning is completely local. Nothing leaves your machine. We're reading files that belong to you, on your computer, as your user account. It's not a hack, it's just reading your own data. Chrome stores it unencrypted. We didn't make the rules.

## How the WebSocket Works

Conduit connects to `wss://wss-primary.slack.com` with your `xoxc-` token in the URL and `d` cookie in the handshake headers. This is the exact same WebSocket endpoint the Slack web client uses. No bot tokens. No app-level tokens. No event subscriptions to configure. Just a raw WebSocket that pushes events in real-time.

Messages, reactions, typing indicators, presence changes — they all arrive via push. Zero polling. The way the gods intended.

## FAQ

**Q: Is this legal?**
A: It uses Slack's public API with your own credentials. It's as legal as opening Slack in a browser. Which is what you're already doing. Relax.

**Q: It reads my browser data??**
A: YOUR browser data. On YOUR computer. Running as YOUR user account. Chrome stores your Slack token in plaintext in a LevelDB file. We just read it so you don't have to open dev tools and paste things like a barbarian. If this bothers you, wait until you find out what Chrome extensions can access.

**Q: Will Slack break this?**
A: Maybe. They could change the WebSocket protocol or block third-party clients. But they've been running this same WebSocket endpoint for years and millions of connections depend on it. We'll cross that bridge when we come to it.

**Q: Why C++ and not Rust/Go/Python?**
A: Because I wanted to. Also because C++ compiles to native code that starts in 200ms and uses 40MB of RAM. Try doing that with Python.

**Q: Why does it look like a terminal?**
A: Because terminals are beautiful and you have bad taste. Also because rendering a terminal-style UI is way simpler than recreating Slack's CSS nightmare, and simpler means fewer bugs.

**Q: Can I use this at work?**
A: Yes. It connects to Slack. It sends messages. It shows messages. It's a Slack client. Your IT department doesn't need to know the details.

**Q: Does it support threads?**
A: Yes. Click the reply count on any message, use Alt+T, or hover and click Reply.

**Q: Does it support emoji reactions?**
A: Yes. Full-color emoji from Slack's CDN, custom workspace emoji included. Hover a message and click React, or use Alt+R.

**Q: Does it support file uploads?**
A: Yes. Ctrl+V to paste from clipboard, drag-and-drop files onto the window, or `/upload path/to/file`.

**Q: It crashed.**
A: Open an issue. Include the log file at `C:\Users\<you>\conduit_debug.log`. Or don't. I'll probably find it myself eventually.

**Q: Does it work on Mac/Linux?**
A: It builds and runs on all three platforms. The auto-login from browser only works on Windows for now. Mac/Linux users paste their token manually (once).

## License

MIT. Do whatever you want with it. Credit is nice but not required. Complaints go to `/dev/null`.

---

*Built with spite, caffeine, and a mass allergy to Electron.*
