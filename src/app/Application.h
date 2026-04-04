#pragma once
#include "ui/UIManager.h"
#include <SDL.h>

// forward declare so we don't drag GL headers into everything
typedef void* SDL_GLContext;

namespace conduit {

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
    void processInput();
    void handleKeyDown(const SDL_KeyboardEvent& key);

    SDL_Window* window_ = nullptr;
    SDL_GLContext gl_context_ = nullptr;
    ui::UIManager ui_;
    bool running_ = false;
    int window_width_ = 1280;
    int window_height_ = 800;
};

} // namespace conduit
