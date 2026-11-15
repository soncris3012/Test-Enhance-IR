cmake_minimum_required(VERSION 3.20)
project(InfraredEnhancement LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Optimization flags
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

# Find Qt6 and OpenCV
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)
find_package(OpenCV REQUIRED)

include_directories(include)

# Core library
add_library(ir_enhance_core STATIC
    include/image_canvas.hpp
    src/guided_filter.cpp
    src/enhancement_pipeline.cpp
    src/image_canvas.cpp
)
target_include_directories(ir_enhance_core PUBLIC include ${OpenCV_INCLUDE_DIRS})
target_link_libraries(ir_enhance_core PUBLIC ${OpenCV_LIBS} Qt6::Core Qt6::Gui Qt6::Widgets)

# CLI test executable
add_executable(ir_enhance_cli
    apps/cli_test.cpp
)
target_link_libraries(ir_enhance_cli PRIVATE ir_enhance_core)

# GUI executable
add_executable(ir_enhance_gui
    apps/main.cpp
    apps/MainWindow.hpp
    apps/MainWindow.cpp
    apps/MainWindow.ui
)
target_link_libraries(ir_enhance_gui PRIVATE ir_enhance_core Qt6::Widgets)
