cmake_minimum_required(VERSION 3.20)

project(VisionSystemCPP LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTORCC ON)

find_package(OpenCV REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Widgets Svg)

find_path(VIMBAX_INCLUDE_DIR
    NAMES VmbCPP/VmbCPP.h
    HINTS
        $ENV{VIMBA_X_HOME}
        $ENV{VIMBAX_HOME}
        "C:/Program Files/Allied Vision/Vimba X"
        "C:/Program Files/Allied Vision/VimbaX"
    PATH_SUFFIXES api/include api/VmbCPP/include include Include
)

find_library(VIMBAX_CPP_LIBRARY
    NAMES VmbCPP VmbCPP_MD VmbCPP64
    HINTS
        $ENV{VIMBA_X_HOME}
        $ENV{VIMBAX_HOME}
        "C:/Program Files/Allied Vision/Vimba X"
        "C:/Program Files/Allied Vision/VimbaX"
    PATH_SUFFIXES api/lib api/VmbCPP/lib lib Lib
)

add_executable(VisionSystemCPP
    src/main.cpp
    src/access/AccessTypes.h
    src/access/AccessTypes.cpp
    src/access/AccessPolicy.h
    src/access/AccessPolicy.cpp
    src/access/AccessSession.h
    src/access/AccessSession.cpp
    src/access/AccessRecipe.h
    src/access/AccessRecipe.cpp
    src/config/AppConfig.h
    src/config/AppConfig.cpp
    src/config/RecipeManager.h
    src/config/RecipeManager.cpp
    src/config/RecipeDeletion.h
    src/config/RecipeDeletion.cpp
    src/config/RecipeManagerAI.cpp
    src/config/RecipeManagerGeometry.cpp
    src/config/RecipeManagerMeasurement.cpp
    src/config/RecipeManagerSurfaceDefect.cpp
    src/config/RecipeManagerSurfaceLocalization.cpp
    src/config/RecipeManagerStorage.cpp
    src/config/RecipeJsonUtils.h
    src/config/RecipeJsonUtils.cpp
    src/config/TranslationManager.h
    src/config/TranslationManager.cpp
    src/ai/AiMaskLabelStorage.h
    src/ai/AiMaskLabelStorage.cpp
    src/calibration/CalibrationTypes.h
    src/calibration/CalibrationPatternDetector.h
    src/calibration/CalibrationPatternDetector.cpp
    src/calibration/PlanarCalibrationEstimator.h
    src/calibration/PlanarCalibrationEstimator.cpp
    src/calibration/CalibrationSolver.h
    src/calibration/CalibrationSolver.cpp
    src/calibration/CalibrationSession.h
    src/calibration/CalibrationSession.cpp
    src/calibration/ImageCalibrationMap.h
    src/calibration/ImageCalibrationMap.cpp
    src/calibration/CalibrationRecipe.h
    src/calibration/CalibrationRecipe.cpp
    src/camera/ICamera.h
    src/camera/CameraDeviceInfo.h
    src/camera/FileCamera.h
    src/camera/FileCamera.cpp
    src/camera/SimulatedCamera.h
    src/camera/SimulatedCamera.cpp
    src/camera/UsbCamera.h
    src/camera/UsbCamera.cpp
    src/camera/UsbCameraDiscovery.h
    src/camera/UsbCameraDiscovery.cpp
    src/camera/VimbaCamera.h
    src/camera/VimbaCamera.cpp
    src/camera/VimbaTriggerController.h
    src/camera/VimbaTriggerController.cpp
    src/camera/VimbaXDiscovery.h
    src/camera/VimbaXDiscovery.cpp
    src/gui/CameraTileWidget.h
    src/gui/CameraTileWidget.cpp
    src/gui/CameraStripWidget.h
    src/gui/CameraStripWidget.cpp
    src/gui/CameraAssignmentDialog.h
    src/gui/CameraAssignmentDialog.cpp
    src/gui/CheckerboardCalibrationDialog.h
    src/gui/CheckerboardCalibrationDialog.cpp
    src/gui/UsbCameraAssignmentDialog.h
    src/gui/UsbCameraAssignmentDialog.cpp
    src/gui/CameraSetupPanelWidget.h
    src/gui/CameraSetupPanelWidget.cpp
    src/gui/CommandToolbarWidget.h
    src/gui/CommandToolbarWidget.cpp
    src/gui/IconCatalog.h
    src/gui/IconCatalog.cpp
    src/gui/ImagePrimitives.h
    src/gui/ImageViewWidget.h
    src/gui/ImageViewWidget.cpp
    src/gui/ImageViewWidgetState.cpp
    src/gui/ImageViewWidgetGeometry.cpp
    src/gui/MeasurementResultsWidget.h
    src/gui/MeasurementResultsWidget.cpp
    src/gui/ToolIconBarWidget.h
    src/gui/ToolIconBarWidget.cpp
    src/gui/TouchIconButton.h
    src/gui/TouchIconButton.cpp
    src/gui/ImageViewWidgetInteraction.cpp
    src/gui/ImageViewWidgetInteractionRelease.cpp
    src/gui/ImageViewWidgetPainting.cpp
    src/gui/ImageViewWidgetPolygon.cpp
    src/gui/help/HelpAssistantService.h
    src/gui/help/HelpAssistantService.cpp
    src/gui/help/HelpAssistantService_Retrieval.cpp
    src/gui/help/HelpAssistantService_DirectReply.cpp
    src/gui/help/HelpModelBootstrapper.h
    src/gui/help/HelpModelBootstrapper.cpp
    src/gui/help/HelpDialog.h
    src/gui/help/HelpDialog.cpp
    src/gui/MainWindow.h
    src/gui/MainWindow.cpp
    src/gui/MainWindowHelp.cpp
    src/gui/MainWindowSystem.cpp
    src/gui/MainWindowMenu.cpp
    src/gui/MainWindowInteractions.cpp
    src/gui/MainWindowPanels.cpp
    src/gui/MainWindowStyle.cpp
    src/gui/MainWindowImageHandlers.cpp
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
    src/gui/modules/MainWindowConstructedGeometryPanels.cpp
    src/gui/modules/MainWindowConstructedGeometryRecipe.cpp
    src/gui/modules/MainWindowImagingModule.cpp
    src/gui/modules/MainWindowRecipeModule.cpp
    src/gui/modules/MainWindowRecipeModule_Delete.cpp
    src/gui/modules/MainWindowLocalizationModule.cpp
    src/gui/modules/MainWindowMeasurementModule.h
    src/gui/modules/MainWindowMeasurementModule.cpp
    src/gui/modules/MainWindowMeasurementPanels.cpp
    src/gui/modules/MainWindowMeasurementActions.cpp
    src/gui/modules/MainWindowMeasurementRecipe.cpp
    src/gui/modules/MainWindowGeometryModule.h
    src/gui/modules/geometry/MainWindowGeometryModule_Recipe.cpp
    src/gui/modules/geometry/MainWindowGeometryModule_Delete.cpp
    src/gui/modules/geometry/GeometryToolPanels.cpp
    src/gui/modules/geometry/GeometryToolPanels_Point.cpp
    src/gui/modules/geometry/GeometryToolPanels_Line.cpp
    src/gui/modules/geometry/GeometryToolPanels_Circle.cpp
    src/gui/modules/geometry/GeometryRuntimeModule.cpp
    src/gui/modules/geometry/GeometryRuntimeModule_Lines.cpp
    src/gui/modules/geometry/GeometryRuntimeModule_Configured.cpp
    src/gui/modules/geometry/GeometryRuntimeModule_PointCircle.cpp
    src/gui/modules/geometry/MainWindowGeometryModule_ArcPanels.cpp
    src/gui/modules/geometry/MainWindowGeometryModule_ArcRuntime.cpp
    src/gui/modules/MainWindowSurfaceModule.h
    src/gui/modules/surface/MainWindowSurfaceModule_Panels.cpp
    src/gui/modules/surface/MainWindowSurfaceModule_LocalizationToolCircleAnnulus.cpp
    src/gui/modules/surface/MainWindowSurfaceModule_LocalizationToolMassPca.cpp
    src/gui/modules/surface/MainWindowSurfaceModule_LocalizationToolEdgePca.cpp
    src/gui/modules/surface/MainWindowSurfaceModule_LocalizationToolModel.cpp
    src/gui/modules/surface/MainWindowSurfaceModule_LocalizationToolPlanned.cpp
    src/gui/modules/surface/MainWindowSurfaceModule_Runtime.cpp
    src/gui/modules/surface/MainWindowSurfaceModule_Model.cpp
    src/gui/modules/setup/SetupResultsDialog.h
    src/gui/modules/setup/SetupResultsDialog.cpp
    src/gui/modules/setup/SetupResultsFormatter.h
    src/gui/modules/setup/SetupResultsFormatter.cpp
    src/gui/modules/setup/SetupCameraResolver.h
    src/gui/modules/setup/SetupCameraResolver.cpp
    src/gui/modules/setup/AiPythonRuntime.h
    src/gui/modules/setup/AiPythonRuntime.cpp
    src/gui/modules/setup/AiClassificationPaths.h
    src/gui/modules/setup/AiClassificationPaths.cpp
    src/gui/modules/setup/AiLocalizationPaths.h
    src/gui/modules/setup/AiLocalizationPaths.cpp
    src/gui/modules/setup/AiModelComparison.h
    src/gui/modules/setup/AiModelComparison.cpp
    src/gui/modules/setup/AiTrainingGraph.h
    src/gui/modules/setup/AiTrainingGraph.cpp
    src/gui/modules/setup/MainWindowSetupModule_AiPanels.cpp
    src/gui/modules/setup/MainWindowSetupModule_AiLocalizationPanel.cpp
    src/gui/modules/setup/MainWindowSetupModule_AiLocalizationInference.cpp
    src/gui/modules/setup/MainWindowSetupModule_AiSegmentationPanels.cpp
    src/gui/modules/setup/MainWindowSetupModule_Ai.cpp
    src/gui/modules/setup/MainWindowSetupModule_AiTraining.cpp
    src/gui/modules/setup/MainWindowSetupModule_AiInference.cpp
    src/gui/modules/setup/MainWindowSetupModule_AiCapture.cpp
    src/gui/modules/setup/MainWindowSetupModule_Acquisition.cpp
    src/gui/modules/setup/MainWindowSetupModule_Runtime.cpp
    src/gui/modules/setup/MainWindowSetupModule_Details.cpp
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
    src/gui/geometry/GeometryOverlayPrimitives.h
    src/gui/geometry/GeometryOverlayPrimitives.cpp
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
    src/measurement/MeasurementGeometry.h
    src/measurement/MeasurementGeometryMath.h
    src/measurement/MeasurementGeometryMath.cpp
    src/measurement/RealMeasurementTypes.h
    src/measurement/MeasurementUnitConverter.h
    src/measurement/MeasurementUnitConverter.cpp
    src/measurement/MeasurementToleranceEvaluator.h
    src/measurement/MeasurementToleranceEvaluator.cpp
    src/processing/ImageProcessor.h
    src/processing/ImageProcessor.cpp
    src/processing/CircleFit.h
    src/processing/CircleFit.cpp
    src/processing/LocalizationProcessor.h
    src/processing/LocalizationProcessor.cpp
    src/processing/MaskPoseEstimator.h
    src/processing/MaskPoseEstimator.cpp
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
    src/processing/ShapeCandidateFilter.h
    src/processing/ShapeCandidateFilter.cpp
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
    src/simulator/SimulatorProtocol.h
    src/simulator/SimulatorBridge.h
    src/simulator/SimulatorBridge.cpp
    src/util/DetailedLogger.h
    src/util/DetailedLogger.cpp
    src/utils/Timer.h
    resources/icons.qrc
)

target_link_libraries(VisionSystemCPP PRIVATE
    ${OpenCV_LIBS}
    Qt6::Widgets
    Qt6::Svg
)

add_executable(VisionSystemTestVision
    tests/testvision/main.cpp
    tests/testvision/TestVisionArtifacts.h
    tests/testvision/TestVisionArtifacts.cpp
)

target_link_libraries(VisionSystemTestVision PRIVATE
    ${OpenCV_LIBS}
    Qt6::Widgets
)

target_include_directories(VisionSystemTestVision PRIVATE
    src
)

target_compile_definitions(VisionSystemTestVision PRIVATE
    PROJECT_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

enable_testing()

add_executable(VisionSystemProcessingTests
    tests/processing/test_mask_pose.cpp
    src/ai/AiMaskLabelStorage.h
    src/ai/AiMaskLabelStorage.cpp
    src/processing/MaskPoseEstimator.h
    src/processing/MaskPoseEstimator.cpp
)

target_link_libraries(VisionSystemProcessingTests PRIVATE
    ${OpenCV_LIBS}
    Qt6::Widgets
)

target_include_directories(VisionSystemProcessingTests PRIVATE
    src
)

add_test(NAME mask_pose_estimator COMMAND VisionSystemProcessingTests)

target_include_directories(VisionSystemCPP PRIVATE
    src
)

target_compile_definitions(VisionSystemCPP PRIVATE
    PROJECT_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

if(VIMBAX_INCLUDE_DIR AND VIMBAX_CPP_LIBRARY)
    target_include_directories(VisionSystemCPP PRIVATE ${VIMBAX_INCLUDE_DIR})
    target_link_libraries(VisionSystemCPP PRIVATE ${VIMBAX_CPP_LIBRARY})
    target_compile_definitions(VisionSystemCPP PRIVATE VISION_WITH_VIMBAX)
    message(STATUS "VimbaX SDK enabled: ${VIMBAX_INCLUDE_DIR}")
    file(GLOB VIMBAX_RUNTIME_DLLS
        "$ENV{VIMBA_X_HOME}/api/bin/*.dll"
        "$ENV{VIMBAX_HOME}/api/bin/*.dll"
        "C:/Program Files/Allied Vision/Vimba X/api/bin/*.dll"
        "C:/Program Files/Allied Vision/VimbaX/api/bin/*.dll"
    )
    if(VIMBAX_RUNTIME_DLLS)
        add_custom_command(TARGET VisionSystemCPP POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${VIMBAX_RUNTIME_DLLS}
                "$<TARGET_FILE_DIR:VisionSystemCPP>"
        )
    endif()
else()
    message(STATUS "VimbaX SDK not found: discovery UI will report SDK unavailable")
endif()

get_filename_component(QT6_INSTALL_PREFIX "${Qt6_DIR}/../../.." ABSOLUTE)
set(QT6_PLUGINS_DIR "${QT6_INSTALL_PREFIX}/plugins")
if(NOT EXISTS "${QT6_PLUGINS_DIR}/imageformats/qsvg.dll" AND DEFINED VCPKG_INSTALLED_DIR AND DEFINED VCPKG_TARGET_TRIPLET)
    set(QT6_PLUGINS_DIR "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/Qt6/plugins")
endif()

add_custom_command(TARGET VisionSystemCPP POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:VisionSystemCPP>/imageformats"
        "$<TARGET_FILE_DIR:VisionSystemCPP>/iconengines"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${QT6_PLUGINS_DIR}/imageformats/qsvg.dll"
        "$<TARGET_FILE_DIR:VisionSystemCPP>/imageformats/qsvg.dll"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${QT6_PLUGINS_DIR}/iconengines/qsvgicon.dll"
        "$<TARGET_FILE_DIR:VisionSystemCPP>/iconengines/qsvgicon.dll"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/translations"
        "$<TARGET_FILE_DIR:VisionSystemCPP>/translations"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/docs/help"
        "$<TARGET_FILE_DIR:VisionSystemCPP>/docs/help"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/ollama/vision-help"
        "$<TARGET_FILE_DIR:VisionSystemCPP>/ollama/vision-help"
)
