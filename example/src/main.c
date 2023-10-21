/**
 * @file main.c
 */

#include "utils.h"

// ================================================================================================================================

int main(void)
{
    LOG("[START]\n");
    {
        app_run();
    }
    LOG("[STOP]\n");
    return 0;
}

// ================================================================================================================================

#if defined(_WIN32)
    #include <Windows.h>

    int WINAPI wWinMain
    (
        [[maybe_unused]] _In_ HINSTANCE hInstance,
        [[maybe_unused]] _In_opt_ HINSTANCE hPrevInstance,
        [[maybe_unused]] _In_ LPWSTR lpCmdLine,
        [[maybe_unused]] _In_ int nShowCmd
    )
    {
        return main();
    }
#endif

// ================================================================================================================================
