cmake_minimum_required(VERSION 3.2)

project(ESPresso-Client-RPI)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++20")

set(SOURCE
        src/main.cpp
        src/boiler/BoilerController.cpp
        vendor/ESPresso-UI/Settings/SettingsManagerDummyImpl.cpp
)

set(INCLUDES
        src
        src/boiler
        vendor
        vendor/cpp-httplib
        vendor/json/single_include
        vendor/lv_drivers
        vendor/lvgl
)

# LVGL
set(LV_CONF_PATH
        ${CMAKE_CURRENT_SOURCE_DIR}/src/lv_conf.h
        CACHE STRING "" FORCE)

set(LV_DRV_CONF_PATH
        ${CMAKE_CURRENT_SOURCE_DIR}/src/lv_drv_conf.h
        CACHE STRING "" FORCE)

add_compile_definitions(LV_CONF_INCLUDE_SIMPLE)
add_compile_definitions(LV_LVGL_H_INCLUDE_SIMPLE)

add_subdirectory(vendor/lvgl)
add_subdirectory(vendor/lv_drivers)

# ESPresso-UI
add_subdirectory(vendor/ESPresso-UI)

include_directories(
        ${INCLUDES}
        ${ESPRESSO-UI-INCLUDES}
)

add_executable(ESPresso-Client ${SOURCE} ${ESPRESSO-UI-SOURCE})

target_link_libraries(ESPresso-Client PRIVATE
        lvgl::lvgl
        lvgl::drivers
        pthread
)
