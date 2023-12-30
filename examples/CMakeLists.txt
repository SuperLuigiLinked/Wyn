# @file examples/CMakeLists.txt

option(WYN_EXAMPLE_C "Enables Wyn C example program" ON)
option(WYN_EXAMPLE_CPP "Enables Wyn C++ example program" ON)

if (WYN_EXAMPLE_C)
    add_subdirectory(example_c)
endif()

if (WYN_EXAMPLE_CPP)
    add_subdirectory(example_cpp)
endif()
