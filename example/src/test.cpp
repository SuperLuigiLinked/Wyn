/**
 * @file example/test.cpp
 */

#include <time.h>
#include <chrono>
#include <thread>
#include <latch>

// ================================================================================================================================

extern "C" void cpp_yield(void);
extern "C" void cpp_yield(void)
{
    std::this_thread::yield();
}

extern "C" void test_cpp(void);
extern "C" void test_cpp(void)
{
    {
        std::latch latch{ 1 };
        latch.arrive_and_wait();

        std::thread{ cpp_yield }.detach();
    #ifndef __APPLE__
        std::jthread{ cpp_yield }.detach();
    #endif
    }

    [[maybe_unused]] volatile time_t a = time(nullptr);
    [[maybe_unused]] volatile clock_t b = clock();
    [[maybe_unused]] volatile auto c = std::chrono::steady_clock::now();
    [[maybe_unused]] volatile auto d = std::chrono::high_resolution_clock::now();
    [[maybe_unused]] volatile auto e = std::chrono::system_clock::now();
    std::this_thread::yield();
    std::this_thread::sleep_for(std::chrono::nanoseconds(100));
    std::this_thread::sleep_until(std::chrono::steady_clock::now() + std::chrono::nanoseconds(100));
}

// ================================================================================================================================
