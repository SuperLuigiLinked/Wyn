/*
    Example - test.cpp
*/

#include <time.h>
#include <chrono>

// ================================================================================================================================ //

extern "C" void test_cpp(void);

extern "C" void test_cpp(void)
{
    [[maybe_unused]] volatile time_t a = time(nullptr);
    [[maybe_unused]] volatile clock_t b = clock();
    [[maybe_unused]] volatile auto c = std::chrono::steady_clock::now();
    [[maybe_unused]] volatile auto d = std::chrono::high_resolution_clock::now();
    [[maybe_unused]] volatile auto e = std::chrono::system_clock::now();
}

// ================================================================================================================================ //
