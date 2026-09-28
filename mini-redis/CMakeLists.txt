cmake_minimum_required(VERSION 3.14)
project(mini_redis CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)

if(MSVC)
    add_compile_options(/utf-8 /W4)       # komentari sadrze hrvatska slova
else()
    add_compile_options(-Wall -Wextra)
endif()

if(WIN32)
    add_compile_definitions(_WIN32_WINNT=0x0601)  # Windows 7+ (treba za inet_pton)
endif()

# Jezgra baze: sve sto ne ovisi o mrezi. Koriste je i server i testovi.
add_library(redis_core
    src/store.cpp
    src/parser.cpp
    src/database.cpp
)
target_include_directories(redis_core PUBLIC src)
target_link_libraries(redis_core PUBLIC Threads::Threads)

# Server
add_executable(mini_redis src/main.cpp src/server.cpp)
target_link_libraries(mini_redis PRIVATE redis_core)

# Klijent
add_executable(mini_redis_client client/client.cpp)
target_link_libraries(mini_redis_client PRIVATE Threads::Threads)

# Benchmark
add_executable(mini_redis_bench bench/bench.cpp)
target_link_libraries(mini_redis_bench PRIVATE Threads::Threads)

# Na Windowsu mrezne funkcije zive u biblioteci ws2_32
if(WIN32)
    target_link_libraries(mini_redis PRIVATE ws2_32)
    target_link_libraries(mini_redis_client PRIVATE ws2_32)
    target_link_libraries(mini_redis_bench PRIVATE ws2_32)
endif()

# Testovi
enable_testing()
add_executable(tests tests/tests.cpp)
target_link_libraries(tests PRIVATE redis_core)
add_test(NAME tests COMMAND tests)
