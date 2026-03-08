#include "app/engine_sim_application.h"

#include <string>

static void runApp(void *handle, ysContextObject::DeviceAPI api, const char *scriptPath = nullptr) {
    EngineSimApplication application;
    if (scriptPath != nullptr) {
        application.setScriptPathOverride(scriptPath);
    }
    application.initialize(handle, api);
    application.run();
    application.destroy();
}

#if _WIN32
int WINAPI WinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPSTR lpCmdLine,
    _In_ int nCmdShow)
{
    (void)nCmdShow;
    (void)lpCmdLine;
    (void)hPrevInstance;

    runApp(static_cast<void*>(&hInstance), ysContextObject::DeviceAPI::DirectX11);

    return 0;
}

#else
int main(int argc, char **argv) {
    const char *scriptPath = nullptr;

    if (argc == 3 && std::string(argv[1]) == "--script") {
        scriptPath = argv[2];
    }

    runApp(nullptr, ysContextObject::DeviceAPI::OpenGL4_0, scriptPath);
    return 0;
}

#endif
