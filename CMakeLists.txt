cmake_minimum_required(VERSION 3.20)

project(MemoryAllocationEngine LANGUAGES CXX)
add_compile_options(-mcx16)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-mcx16)
endif()

add_executable(benchmark
    src/benchmark.cpp
    src/slab_allocator.cpp
    src/thread_cache.cpp
    src/central_pool.cpp
)

target_include_directories(benchmark PRIVATE include)

if(MSVC)
    target_compile_options(benchmark PRIVATE /W4)
else()
    target_compile_options(benchmark PRIVATE -Wall -Wextra -Wpedantic)
endif()
target_link_libraries(benchmark PRIVATE atomic)