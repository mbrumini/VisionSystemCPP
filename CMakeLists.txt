cmake_minimum_required(VERSION 3.20)

project(VisionSystemCPP LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(OpenCV REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_executable(VisionSystemCPP
    src/main.cpp
    src/config/AppConfig.h
    src/config/AppConfig.cpp
    src/config/RecipeManager.h
    src/config/RecipeManager.cpp
    src/config/TranslationManager.h
    src/config/TranslationManager.cpp
    src/camera/ICamera.h
    src/camera/FileCamera.h
    src/camera/FileCamera.cpp
    src/gui/CameraTileWidget.h
    src/gui/CameraTileWidget.cpp
    src/gui/ImageViewWidget.h
    src/gui/ImageViewWidget.cpp
    src/gui/MainWindow.h
    src/gui/MainWindow.cpp
    src/gui/SurfaceLocalizationPanelWidget.h
    src/gui/SurfaceLocalizationPanelWidget.cpp
    src/gui/SurfaceLocalizationStrategies.h
    src/gui/SurfaceLocalizationStrategies.cpp
    src/gui/ToolCatalog.h
    src/gui/ToolCatalog.cpp
    src/gui/ToolPanelWidget.h
    src/gui/ToolPanelWidget.cpp
    src/processing/ImageProcessor.h
    src/processing/ImageProcessor.cpp
    src/processing/CircleFit.h
    src/processing/CircleFit.cpp
    src/processing/LocalizationProcessor.h
    src/processing/LocalizationProcessor.cpp
    src/processing/SurfaceDefectProcessor.h
    src/processing/SurfaceDefectProcessor.cpp
    src/processing/SurfaceEdgeStrategy.h
    src/processing/SurfaceEdgeStrategy.cpp
    src/processing/SurfaceModelStrategy.h
    src/processing/SurfaceModelStrategy.cpp
    src/processing/SurfaceModelTrainer.h
    src/processing/SurfaceModelTrainer.cpp
    src/processing/SurfaceProcessingUtils.h
    src/processing/SurfacePcaStrategy.h
    src/processing/SurfacePcaStrategy.cpp
    src/processing/SurfaceThresholdStrategy.h
    src/processing/SurfaceThresholdStrategy.cpp
    src/processing/SurfaceTwoCirclesStrategy.h
    src/processing/SurfaceTwoCirclesStrategy.cpp
    src/utils/Timer.h
)

target_link_libraries(VisionSystemCPP PRIVATE
    ${OpenCV_LIBS}
    Qt6::Widgets
)

target_include_directories(VisionSystemCPP PRIVATE
    src
)

target_compile_definitions(VisionSystemCPP PRIVATE
    PROJECT_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

add_custom_command(TARGET VisionSystemCPP POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/translations"
        "$<TARGET_FILE_DIR:VisionSystemCPP>/translations"
)
