cmake_minimum_required(VERSION 3.14)
project(vk_console_messenger)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

include(FetchContent)

FetchContent_Declare(json 
    URL https://github.com/nlohmann/json/releases/download/v3.11.2/json.tar.xz
)
FetchContent_MakeAvailable(json)

FetchContent_Declare(cpr 
    GIT_REPOSITORY https://github.com/libcpr/cpr.git
    GIT_TAG 1.10.x
)
FetchContent_MakeAvailable(cpr)

add_executable(${PROJECT_NAME} 
    src/main.cpp 
    src/app.cpp
    src/vk_api.cpp
    src/longpoll.cpp
    src/ui_console.cpp 
)

target_include_directories(${PROJECT_NAME} PRIVATE 
    include 
    libs
)

target_link_libraries(${PROJECT_NAME} PRIVATE 
    cpr::cpr 
    nlohmann_json::nlohmann_json
    Threads::Threads
    notcurses
    notcurses-core
)