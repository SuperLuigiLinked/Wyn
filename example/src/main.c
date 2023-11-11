/**
 * @file main.c
 */

#include "app.h"

// ================================================================================================================================

#if defined(_WIN32)
#include <Windows.h>
int WINAPI wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int)
#else
int main(void)
#endif
{
    LOG("[START]\n");
    {
        static App app = {};
        wyn_run(&app);
    }
    LOG("[STOP]\n");
    return EXIT_SUCCESS;
}

// ================================================================================================================================
