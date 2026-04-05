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
- **Unicode emoji rendering** via system fonts. Yes, your coworkers' reaction spam renders fine
- **IRC-style messages** — `20:34 <alice> hey everyone` — because this is how chat should look
- **Click-to-view images** in a full-screen overlay, click-to-toggle reactions
- **Threads, search, file upload, clipboard paste** (Ctrl+V an image, it just works)
- **Right-click context menus** on messages, users, and channels
- **Typing indicators, presence dots, unread badges** — all the stuff you'd expect
- **Keyboard-driven everything** — Alt+1-9, /slash commands, tab completion, Ctrl+K command palette
- **Draggable resizable panes** — sidebar widths adjust, window size persists between sessions
- **Zero-config authentication** — paste your token from the browser, done forever
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

### The Fast Way (you have Slack in a browser)

1. Build Conduit (see below)
2. Run it
3. It'll ask you to paste your token. Get it from the browser:
   - Open Slack in your browser (app.slack.com)
   - Press F12 → Console tab
   - Paste this one-liner:
     ```js
     JSON.parse(localStorage.localConfig_v2).teams[Object.keys(JSON.parse(localStorage.localConfig_v2).teams)[0]].token
     ```
   - Copy the `xoxc-...` token it spits out
4. Paste it into Conduit, hit Enter
5. Now it asks for the `d` cookie:
   - F12 → Application tab → Cookies → `.slack.com` → find `d` → copy the value
6. Paste it, hit Enter
7. You're in. Token is saved to Windows Credential Manager. You won't have to do this again.

### The Slow Way (you have a Slack App configured)

Put your tokens in `%APPDATA%\conduit\conduit.toml`:

```toml
[[org]]
name = "MyWorkspace"
user_token = "xoxp-your-token-here"
app_token = "xapp-your-app-token-here"
auto_connect = true
```

## Building

### Prerequisites

- CMake 3.24+
- MSVC 2022 (or GCC 12+, Clang 15+)
- vcpkg

### Setup

```bash
git clone https://github.com/microsoft/vcpkg.git C:/vcpkg
C:/vcpkg/bootstrap-vcpkg.bat
```

### Build

```bash
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Release
```

### Run

```
build\Release\conduit.exe
```

That's it. No `npm install`. No `yarn`. No `node_modules` folder the size of a small country.

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
/msg @user text     Send a DM
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
- **Dear ImGui** for the UI (immediate mode, 60fps, ~11K lines of actual code)
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

## How the WebSocket works

Conduit connects to `wss://wss-primary.slack.com` with your `xoxc-` token in the URL and `d` cookie in the handshake headers. This is the exact same WebSocket endpoint the Slack web client uses. No bot tokens. No app-level tokens. No event subscriptions to configure. Just a raw WebSocket that pushes events in real-time.

Messages, reactions, typing indicators, presence changes — they all arrive via push. Zero polling. The way the gods intended.

## FAQ

**Q: Is this legal?**
A: It uses Slack's public API with your own credentials. It's as legal as opening Slack in a browser. Which is what you're already doing. Relax.

**Q: Will Slack break this?**
A: Maybe. They could change the WebSocket protocol or block third-party clients. But they've been running this same WebSocket endpoint for years and millions of connections depend on it. We'll cross that bridge when we come to it.

**Q: Why C++ and not Rust/Go/Python?**
A: Because I wanted to. Also because C++ compiles to native code that starts in 200ms and uses 40MB of RAM. Try doing that with Python.

**Q: Why does it look like a terminal?**
A: Because terminals are beautiful and you have bad taste. Also because rendering a terminal-style UI is way simpler than recreating Slack's CSS nightmare, and simpler means fewer bugs.

**Q: Can I use this at work?**
A: Yes. It connects to Slack. It sends messages. It shows messages. It's a Slack client. Your IT department doesn't need to know the details.

**Q: Does it support threads?**
A: Yes. Alt+T or right-click → Reply in thread.

**Q: Does it support emoji reactions?**
A: Yes. Alt+R for the picker, or click existing reaction badges to toggle. Or `/react :emoji:`.

**Q: Does it support file uploads?**
A: Yes. Ctrl+V to paste from clipboard, drag-and-drop files onto the window, or `/upload path/to/file`.

**Q: My messages are showing up twice.**
A: That was a bug. It's fixed. Update your build.

**Q: It crashed.**
A: Open an issue. Include the log file at `C:\Users\<you>\conduit_debug.log`. Or don't. I'll probably find it myself eventually.

## License

MIT. Do whatever you want with it. Credit is nice but not required. Complaints go to `/dev/null`.

---

*Built with spite, caffeine, and a mass allergy to Electron.*
