/**
 * @file main.cpp
 */

#include "app.hpp"

// ================================================================================================================================

inline static int app_main(void)
{
    LOG("[START | EXAMPLE-CPP]\n");
    {
        static App app = {};
        wyn_run(&app);
    }
    LOG("[STOP | EXAMPLE-CPP]\n");
    return EXIT_SUCCESS;
}

#if defined(_WIN32)
    #define WIN32_LEAN_AND_MEAN
    #include <Windows.h>
    int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nShowCmd)
    {
        (void)hInstance; (void)hPrevInstance; (void)lpCmdLine; (void)nShowCmd;
        return app_main();
    }
#else
    int main(void)
    {
        return app_main();
    }
#endif

// ================================================================================================================================
