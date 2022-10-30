cmake_minimum_required(VERSION 3.2)

project(ESPresso-Client-Sim)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++20")

set(SOURCE
        main.cpp
        mouse_cursor_icon.c
        ../src/boiler/BoilerController.cpp
        ../src/scales/ScalesController.cpp
        ../vendor/ESPresso-UI/Settings/SettingsManagerFile.cpp
)

set(INCLUDES
        ../src/boiler
        ../src/scales
        ../vendor
        ../vendor/cpp-httplib
        ../vendor/json/single_include
        ../vendor/lv_drivers
        ../vendor/lvgl
)

# LVGL
set(LV_CONF_PATH
        ${CMAKE_CURRENT_SOURCE_DIR}/lv_conf.h
        CACHE STRING "" FORCE)

set(LV_DRV_CONF_PATH
        ${CMAKE_CURRENT_SOURCE_DIR}/lv_drv_conf.h
        CACHE STRING "" FORCE)

add_compile_definitions(LV_CONF_INCLUDE_SIMPLE)
add_compile_definitions(LV_LVGL_H_INCLUDE_SIMPLE)

add_subdirectory(../vendor/lvgl ${CMAKE_CURRENT_BINARY_DIR}/lvgl)

find_package(SDL2 REQUIRED SDL2)
add_library(lvgl-sdl ../vendor/lv_drivers/sdl/sdl.c)
target_include_directories(lvgl-sdl PUBLIC ${SDL2_INCLUDE_DIRS}../ ${CMAKE_SOURCE_DIR})

# ESPresso-UI
add_subdirectory(../vendor/ESPresso-UI ${CMAKE_CURRENT_BINARY_DIR}/espresso-ui)

include_directories(
        ${INCLUDES}
        ${ESPRESSO-UI-INCLUDES}
        ${SDL2_INCLUDE_DIRS}/..
)

add_executable(ESPresso-Client-SDL
        ${SOURCE}
        ${ESPRESSO-UI-SOURCE}
)

target_link_libraries(ESPresso-Client-SDL PRIVATE
        lvgl::lvgl
        lvgl-sdl
        pthread
        ${SDL2_LIBRARIES}
)
