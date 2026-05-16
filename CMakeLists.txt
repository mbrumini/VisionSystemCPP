cmake_minimum_required(VERSION 3.20)

project(VisionSystemCPP LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(OpenCV REQUIRED)

add_executable(VisionSystemCPP
    src/main.cpp
    src/camera/ICamera.h
    src/camera/FileCamera.h
    src/camera/FileCamera.cpp
    src/processing/ImageProcessor.h
    src/processing/ImageProcessor.cpp
    src/utils/Timer.h
)

target_link_libraries(VisionSystemCPP PRIVATE ${OpenCV_LIBS})

target_compile_definitions(VisionSystemCPP PRIVATE
    PROJECT_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)
