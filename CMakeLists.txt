cmake_minimum_required(VERSION 3.20)
project(InfraredEnhancement LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Optimization & warning flags (cross-platform)
if(MSVC)
    add_compile_options(/W4 /O2)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Find Qt6 and OpenCV
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)
find_package(OpenCV REQUIRED)

# Pure C++17 + OpenCV algorithm core library (no Qt GUI dependency)
add_library(ir_enhance_core STATIC
    include/guided_filter.hpp
    include/enhancement_pipeline.hpp
    src/guided_filter.cpp
    src/enhancement_pipeline.cpp
)
target_include_directories(ir_enhance_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${OpenCV_INCLUDE_DIRS}
)
target_link_libraries(ir_enhance_core PUBLIC ${OpenCV_LIBS})

# CLI benchmark executable (headless, Qt-free)
add_executable(ir_enhance_cli
    apps/cli_test.cpp
)
target_link_libraries(ir_enhance_cli PRIVATE ir_enhance_core)

# Qt 6 GUI executable
add_executable(ir_enhance_gui
    apps/main.cpp
    apps/MainWindow.hpp
    apps/MainWindow.cpp
    apps/MainWindow.ui
    include/image_canvas.hpp
    src/image_canvas.cpp
)
target_link_libraries(ir_enhance_gui PRIVATE
    ir_enhance_core
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)
set_target_properties(ir_enhance_gui PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
)
