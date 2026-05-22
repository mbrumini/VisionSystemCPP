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
    src/config/RecipeManagerGeometry.cpp
    src/config/RecipeManagerStorage.cpp
    src/config/RecipeJsonUtils.h
    src/config/RecipeJsonUtils.cpp
    src/config/TranslationManager.h
    src/config/TranslationManager.cpp
    src/camera/ICamera.h
    src/camera/FileCamera.h
    src/camera/FileCamera.cpp
    src/gui/CameraTileWidget.h
    src/gui/CameraTileWidget.cpp
    src/gui/CameraSetupPanelWidget.h
    src/gui/CameraSetupPanelWidget.cpp
    src/gui/ImagePrimitives.h
    src/gui/ImageViewWidget.h
    src/gui/ImageViewWidget.cpp
    src/gui/ImageViewWidgetInteraction.cpp
    src/gui/ImageViewWidgetPainting.cpp
    src/gui/MainWindow.h
    src/gui/MainWindow.cpp
    src/gui/MainWindowUi.cpp
    src/gui/modules/MainWindowCameraProfile.cpp
    src/gui/modules/ConstructedGeometryLineSource.h
    src/gui/modules/ConstructedGeometryLineSource.cpp
    src/gui/modules/ConstructedGeometryCircleSource.h
    src/gui/modules/ConstructedGeometryCircleSource.cpp
    src/gui/modules/ConstructedGeometryPointSource.h
    src/gui/modules/ConstructedGeometryPointSource.cpp
    src/gui/modules/MainWindowConstructedGeometryModule.h
    src/gui/modules/MainWindowConstructedGeometryModule.cpp
    src/gui/modules/MainWindowConstructedGeometryRecipe.cpp
    src/gui/modules/MainWindowImagingModule.cpp
    src/gui/modules/MainWindowRecipeModule.cpp
    src/gui/modules/MainWindowLocalizationModule.cpp
    src/gui/modules/MainWindowGeometryModule.h
    src/gui/modules/geometry/MainWindowGeometryModule_Recipe.cpp
    src/gui/modules/geometry/GeometryToolPanels.cpp
    src/gui/modules/geometry/GeometryRuntimeModule.cpp
    src/gui/modules/geometry/MainWindowGeometryModule_ArcPanels.cpp
    src/gui/modules/geometry/MainWindowGeometryModule_ArcRuntime.cpp
    src/gui/modules/MainWindowSurfaceModule.h
    src/gui/modules/surface/MainWindowSurfaceModule_Panels.cpp
    src/gui/modules/surface/MainWindowSurfaceModule_Runtime.cpp
    src/gui/modules/surface/MainWindowSurfaceModule_Model.cpp
    src/gui/modules/MainWindowSetupModule.cpp
    src/gui/modules/MainWindowCameraConfigModule.cpp
    src/gui/SurfaceLocalizationPanelWidget.h
    src/gui/SurfaceLocalizationPanelWidget.cpp
    src/gui/SurfaceLocalizationAdapters.h
    src/gui/SurfaceLocalizationAdapters.cpp
    src/gui/SurfaceLocalizationStrategies.h
    src/gui/SurfaceLocalizationStrategies.cpp
    src/gui/ToolCatalog.h
    src/gui/ToolCatalog.cpp
    src/gui/ToolPanelWidget.h
    src/gui/ToolPanelWidget.cpp
    src/gui/geometry/GeometryOverlay.h
    src/gui/geometry/GeometryOverlay.cpp
    src/gui/geometry/GeometryDiagnosticDrawing.h
    src/gui/geometry/GeometryDiagnosticDrawing.cpp
    src/gui/geometry/GeometryMath.h
    src/gui/geometry/GeometryMath.cpp
    src/gui/geometry/GeometryRuntimeConfig.h
    src/gui/geometry/BandGeometryEditor.h
    src/gui/geometry/BandGeometryEditor.cpp
    src/gui/geometry/LineGeometryEditor.h
    src/gui/geometry/LineGeometryEditor.cpp
    src/gui/geometry/LineGeometryMouseController.h
    src/gui/geometry/LineGeometryMouseController.cpp
    src/geometry/GeometryCommon.h
    src/geometry/PointGeometry.h
    src/geometry/PointGeometry.cpp
    src/geometry/LineGeometry.h
    src/geometry/LineGeometry.cpp
    src/geometry/CircleGeometry.h
    src/geometry/CircleGeometry.cpp
    src/geometry/ArcGeometry.h
    src/geometry/ArcGeometry.cpp
    src/geometry/ConstructedGeometry.h
    src/geometry/ConstructedGeometry.cpp
    src/geometry/ConstructedGeometryMath.h
    src/geometry/ConstructedGeometryMath.cpp
    src/geometry/EdgeGeometry.h
    src/geometry/EdgeGeometry.cpp
    src/geometry/ContourGeometry.h
    src/geometry/ContourGeometry.cpp
    src/geometry/GeometrySet.h
    src/geometry/GeometrySet.cpp
    src/processing/ImageProcessor.h
    src/processing/ImageProcessor.cpp
    src/processing/CircleFit.h
    src/processing/CircleFit.cpp
    src/processing/LocalizationProcessor.h
    src/processing/LocalizationProcessor.cpp
    src/processing/geometry/GeometryDetectorCommon.h
    src/processing/geometry/ThresholdMaskBuilder.h
    src/processing/geometry/ThresholdMaskBuilder.cpp
    src/processing/geometry/ThresholdCircleDetector.h
    src/processing/geometry/ThresholdCircleDetector.cpp
    src/processing/geometry/ThresholdLineDetector.h
    src/processing/geometry/ThresholdLineDetector.cpp
    src/processing/geometry/EdgePointFilters.h
    src/processing/geometry/EdgePointFilters.cpp
    src/processing/geometry/EdgePointDetector.h
    src/processing/geometry/EdgePointDetector.cpp
    src/processing/geometry/EdgeLineDetector.h
    src/processing/geometry/EdgeLineDetector.cpp
    src/processing/geometry/EdgeCircleDetector.h
    src/processing/geometry/EdgeCircleDetector.cpp
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
    src/runtime/CameraRuntime.h
    src/runtime/CameraRuntime.cpp
    src/runtime/PartPose.h
    src/util/DetailedLogger.h
    src/util/DetailedLogger.cpp
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
