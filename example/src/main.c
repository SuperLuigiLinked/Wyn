/**
 * @file main.c
 */

#include "utils.h"

// ================================================================================================================================

struct Test
{
    wyt_sem_t sem;
};
typedef struct Test Test;

static void test_sem(void)
{
    Test test = {};

    LOG("[MAIN] CREATING\n");
    test.sem = wyt_sem_create(1, 0);
    ASSERT(test.sem != 0);
    {
        ASSERT(false == wyt_sem_try_acquire(test.sem));
        wyt_sem_release(test.sem);
        ASSERT(true == wyt_sem_try_acquire(test.sem));
    }
    LOG("[MAIN] DESTROYING\n");
    wyt_sem_destroy(test.sem);
}

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
        test_sem();
        // events_loop();
    }
    LOG("[STOP]\n");
    return 0;
}

// ================================================================================================================================
