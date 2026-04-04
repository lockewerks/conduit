#include "app/Application.h"
#include "util/Logger.h"
#include "util/Platform.h"

#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_opengl3.h>

#ifdef _WIN32
#include <windows.h>
#endif
#include <GL/gl.h>

#include <filesystem>

namespace conduit {

Application::Application() {}

Application::~Application() {
    shutdown();
}

bool Application::init() {
    LOG_INFO("conduit starting up...");

    if (!initSDL()) return false;
    if (!initOpenGL()) return false;
    if (!initImGui()) return false;

    loadFonts();
    ui_.theme().apply();

    running_ = true;
    LOG_INFO("init complete, let's go");
    return true;
}

bool Application::initSDL() {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        LOG_ERROR(std::string("SDL_Init failed: ") + SDL_GetError());
        return false;
    }

    // opengl 3.3 core. if your GPU can't do this, it's time to upgrade.
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

    window_ = SDL_CreateWindow(
        "Conduit",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        window_width_, window_height_,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);

    if (!window_) {
        LOG_ERROR(std::string("SDL_CreateWindow failed: ") + SDL_GetError());
        return false;
    }

    LOG_INFO("SDL window created");
    return true;
}

bool Application::initOpenGL() {
    gl_context_ = SDL_GL_CreateContext(window_);
    if (!gl_context_) {
        LOG_ERROR(std::string("GL context creation failed: ") + SDL_GetError());
        return false;
    }

    SDL_GL_MakeCurrent(window_, gl_context_);
    SDL_GL_SetSwapInterval(1); // vsync on, we're not rendering a game

    LOG_INFO("OpenGL context ready");
    return true;
}

bool Application::initImGui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    // docking would be cool for split buffers later but let's not overcomplicate things yet
    // io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    ImGui_ImplSDL2_InitForOpenGL(window_, gl_context_);
    ImGui_ImplOpenGL3_Init("#version 330");

    LOG_INFO("Dear ImGui initialized");
    return true;
}

void Application::loadFonts() {
    ImGuiIO& io = ImGui::GetIO();

    // try to find the font relative to the executable first, then assets/
    std::string font_path = "assets/fonts/JetBrainsMono-Regular.ttf";

    if (!std::filesystem::exists(font_path)) {
        // maybe we're running from the build directory
        font_path = "../assets/fonts/JetBrainsMono-Regular.ttf";
    }

    if (std::filesystem::exists(font_path)) {
        io.Fonts->AddFontFromFileTTF(font_path.c_str(), 14.0f);
        LOG_INFO("loaded JetBrains Mono from " + font_path);
    } else {
        LOG_WARN("couldn't find JetBrains Mono, falling back to imgui default font (gross)");
    }
}

void Application::run() {
    while (running_) {
        processInput();

        // start the dear imgui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        // render our ui
        ui_.render();

        // actually draw everything
        ImGui::Render();
        int display_w, display_h;
        SDL_GL_GetDrawableSize(window_, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);

        auto& bg = ui_.theme().bg_main;
        glClearColor(bg.x, bg.y, bg.z, bg.w);
        glClear(GL_COLOR_BUFFER_BIT);

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window_);
    }
}

void Application::processInput() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        ImGui_ImplSDL2_ProcessEvent(&event);

        switch (event.type) {
        case SDL_QUIT:
            running_ = false;
            break;

        case SDL_WINDOWEVENT:
            if (event.window.event == SDL_WINDOWEVENT_CLOSE) {
                running_ = false;
            }
            break;

        case SDL_KEYDOWN:
            handleKeyDown(event.key);
            break;
        }
    }
}

void Application::handleKeyDown(const SDL_KeyboardEvent& key) {
    // don't intercept if imgui is eating the input (like when typing in the input bar)
    ImGuiIO& io = ImGui::GetIO();

    switch (key.keysym.sym) {
    case SDLK_F5:
    case SDLK_F6:
        ui_.toggleBufferList();
        break;
    case SDLK_F7:
    case SDLK_F8:
        ui_.toggleNickList();
        break;

    // alt+left/right to switch buffers (weechat style)
    case SDLK_LEFT:
        if (key.keysym.mod & KMOD_ALT) {
            ui_.bufferList().selectPrev();
        }
        break;
    case SDLK_RIGHT:
        if (key.keysym.mod & KMOD_ALT) {
            ui_.bufferList().selectNext();
        }
        break;

    // alt+1-9 for quick buffer switching
    case SDLK_1: case SDLK_2: case SDLK_3: case SDLK_4: case SDLK_5:
    case SDLK_6: case SDLK_7: case SDLK_8: case SDLK_9:
        if (key.keysym.mod & KMOD_ALT) {
            int index = key.keysym.sym - SDLK_1 + 1; // skip org header at 0
            ui_.bufferList().select(index);
        }
        break;

    case SDLK_ESCAPE:
        // escape clears focus or closes overlays
        // for now it just... exists. we'll use it later.
        break;
    }
}

void Application::shutdown() {
    if (gl_context_) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplSDL2_Shutdown();
        ImGui::DestroyContext();
        SDL_GL_DeleteContext(gl_context_);
        gl_context_ = nullptr;
    }
    if (window_) {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }
    SDL_Quit();
    LOG_INFO("shutdown complete, see you next time");
}

} // namespace conduit
