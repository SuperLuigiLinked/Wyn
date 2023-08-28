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

static wyt_retval_t WYT_ENTRY test_thread(void* arg)
{
    LOG("[TEST] START\n");
    {
        Test* const test = arg;
        ASSERT(test != 0);

        LOG("[TEST] SLEEPING\n");
        wyt_nanosleep_for(2'000'000'000ull);

        LOG("[TEST] RELEASING\n");
        wyt_sem_release(test->sem);
    }
    LOG("[TEST] STOP\n");
    return 0;
}

static void test_sem(void)
{
    Test test = {};

    LOG("[MAIN] CREATING\n");
    test.sem = wyt_sem_create(1, 0);
    ASSERT(test.sem != 0);
    {
        LOG("[MAIN] SPAWNING\n");
        const wyt_thread_t thread = wyt_spawn(test_thread, &test);
        ASSERT(thread != 0);
        {
            LOG("[MAIN] ACQUIRING\n");
            wyt_sem_acquire(test.sem);
        }

        LOG("[MAIN] JOINING\n");
        (void)wyt_join(thread);
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
