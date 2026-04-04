# Conduit

A Slack client for people who think the official client uses too much RAM. (It does.)

Conduit is a WeeChat-style desktop Slack client built in C++ with Dear ImGui. It looks like a terminal, acts like a terminal, but secretly it's a full GUI application that can render images, GIFs, and emoji inline. Think of it as WeeChat and Slack having a baby that inherited all the good genes.

## Why?

Because Electron ate my RAM and I want it back. Also because WeeChat keybindings are objectively superior to clicking around with a mouse like some kind of animal.

## What it does

- Full Slack feature parity (messages, threads, reactions, files, search, the works)
- WeeChat-style layout with buffer list, nick list, and keyboard-driven everything
- Inline images and GIFs because we're not savages
- Multi-org support so you can juggle all your Slack workspaces from one window
- Custom emoji rendering because your coworkers' custom emoji deserve respect
- Keyboard-first, mouse-optional (Alt+1-9, /slash commands, tab completion)

## What it doesn't do

- Use Electron (you're welcome)
- Look like Slack (that's the point)
- Support IRC (this isn't WeeChat, it just looks like it)

## Building

### Prerequisites

- CMake 3.24+
- A C++20 compiler (MSVC 2022, GCC 12+, Clang 15+)
- vcpkg (for dependency management)

### Setup vcpkg

If you don't have vcpkg already:

```bash
git clone https://github.com/microsoft/vcpkg.git
cd vcpkg && bootstrap-vcpkg.bat  # or ./bootstrap-vcpkg.sh on linux/mac
```

Set the `VCPKG_ROOT` environment variable to the vcpkg directory.

### Build

```bash
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake
cmake --build build
```

### Run

```bash
./build/conduit          # linux/mac
build\Debug\conduit.exe  # windows
```

## Slack App Setup

You'll need to create a Slack App to use Conduit. Don't worry, it's less painful than it sounds.

1. Go to https://api.slack.com/apps and create a new app from scratch
2. Name it "Conduit" (or whatever, I'm not your boss)
3. Under **OAuth & Permissions**, add these user token scopes:
   - `channels:read`, `channels:write`, `channels:history`
   - `groups:read`, `groups:write`, `groups:history`
   - `im:read`, `im:write`, `im:history`
   - `mpim:read`, `mpim:write`, `mpim:history`
   - `users:read`, `users:read.email`, `users:write`
   - `reactions:read`, `reactions:write`
   - `pins:read`, `pins:write`
   - `files:read`, `files:write`
   - `search:read`, `emoji:read`, `chat:write`, `usergroups:read`
4. Install the app to your workspace and copy the `xoxp-` User OAuth Token
5. Enable **Socket Mode** and generate an App-Level Token with `connections:write` scope
6. Under **Event Subscriptions**, subscribe to the events you care about (message.channels, reaction_added, etc.)
7. Run Conduit and it'll ask for your tokens on first launch

## Keybindings

If you've used WeeChat, you already know most of these.

| Key | What it does |
|-----|-------------|
| `Alt+1..9` | Switch buffers |
| `Alt+A` | Jump to next unread |
| `Alt+Left/Right` | Navigate buffers |
| `Ctrl+K` | Command palette |
| `Ctrl+F` | Search |
| `F5/F6` | Toggle buffer list |
| `F7/F8` | Toggle nick list |
| `Alt+T` | Open thread |
| `Alt+R` | React to message |
| `Tab` | Autocomplete everything |

Full keybinding list: see `docs/KEYBINDINGS.md` (eventually)

## Config

Config lives at `~/.config/conduit/conduit.toml`. See `config/conduit.example.toml` for all the knobs you can turn.

## Status

Very much a work in progress. If it crashes, that's a feature.

## License

MIT. Do whatever you want with it.
