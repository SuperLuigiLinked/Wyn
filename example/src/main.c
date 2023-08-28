/**
 * @file main.c
 */

#include "utils.h"

// ================================================================================================================================

#if defined(_WIN32) && 0
int WINAPI wWinMain
(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR lpCmdLine,
    _In_ int nShowCmd
)
#else
int main(void)
#endif
{
    LOG("[START]\n");
    {
        events_loop();
    }
    LOG("[STOP]\n");
    return 0;
}

// ================================================================================================================================
