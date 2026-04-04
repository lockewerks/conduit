#include "app/Application.h"
#include "util/Logger.h"

// the entry point. not much to see here.
// SDL2 handles the WinMain/main mess for us via SDL2main.

int main(int argc, char* argv[]) {
    conduit::Application app;

    if (!app.init()) {
        LOG_ERROR("failed to initialize, bailing out");
        return 1;
    }

    app.run();
    app.shutdown();

    return 0;
}
