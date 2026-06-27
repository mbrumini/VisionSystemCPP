# Source file map / Mappa dei file sorgente

IT: mappa rapida dei file sotto `src/`. Serve per capire dove guardare quando si modifica un modulo.

EN: quick map of the files under `src/`. Use it to find the right area before changing a module.

> Aggiornare questa mappa quando cambia il ruolo di un file. / Update this map when a file role changes.


## Accesso / Access

| File | IT | EN |
| --- | --- | --- |
| `src/access/AccessPolicy.cpp` | Implementa supporto per access policy. | Implements support code for access policy. |
| `src/access/AccessPolicy.h` | Dichiara supporto per access policy. | Declares support code for access policy. |
| `src/access/AccessRecipe.cpp` | Implementa persistenza/configurazione per access recipe. | Implements persistence/configuration logic for access recipe. |
| `src/access/AccessRecipe.h` | Dichiara persistenza/configurazione per access recipe. | Declares persistence/configuration logic for access recipe. |
| `src/access/AccessSession.cpp` | Implementa supporto per access session. | Implements support code for access session. |
| `src/access/AccessSession.h` | Dichiara supporto per access session. | Declares support code for access session. |
| `src/access/AccessTypes.cpp` | Implementa supporto per access types. | Implements support code for access types. |
| `src/access/AccessTypes.h` | Dichiara supporto per access types. | Declares support code for access types. |

## AI

| File | IT | EN |
| --- | --- | --- |
| `src/ai/AiMaskLabelStorage.cpp` | Implementa supporto AI per ai mask label storage. | Implements AI support for ai mask label storage. |
| `src/ai/AiMaskLabelStorage.h` | Dichiara supporto AI per ai mask label storage. | Declares AI support for ai mask label storage. |

## Calibrazione / Calibration

| File | IT | EN |
| --- | --- | --- |
| `src/calibration/CalibrationPatternDetector.cpp` | Implementa il detector o riconoscitore calibration pattern detector. | Implements the detector/recognizer for calibration pattern detector. |
| `src/calibration/CalibrationPatternDetector.h` | Dichiara il detector o riconoscitore calibration pattern detector. | Declares the detector/recognizer for calibration pattern detector. |
| `src/calibration/CalibrationRecipe.cpp` | Implementa persistenza/configurazione per calibration recipe. | Implements persistence/configuration logic for calibration recipe. |
| `src/calibration/CalibrationRecipe.h` | Dichiara persistenza/configurazione per calibration recipe. | Declares persistence/configuration logic for calibration recipe. |
| `src/calibration/CalibrationSession.cpp` | Implementa logica calibrazione per calibration session. | Implements calibration logic for calibration session. |
| `src/calibration/CalibrationSession.h` | Dichiara logica calibrazione per calibration session. | Declares calibration logic for calibration session. |
| `src/calibration/CalibrationSolver.cpp` | Implementa logica calibrazione per calibration solver. | Implements calibration logic for calibration solver. |
| `src/calibration/CalibrationSolver.h` | Dichiara logica calibrazione per calibration solver. | Declares calibration logic for calibration solver. |
| `src/calibration/CalibrationTypes.h` | Dichiara logica calibrazione per calibration types. | Declares calibration logic for calibration types. |
| `src/calibration/ImageCalibrationMap.cpp` | Implementa logica calibrazione per image calibration map. | Implements calibration logic for image calibration map. |
| `src/calibration/ImageCalibrationMap.h` | Dichiara logica calibrazione per image calibration map. | Declares calibration logic for image calibration map. |
| `src/calibration/PlanarCalibrationEstimator.cpp` | Implementa dati o logica geometrica per planar calibration estimator. | Implements geometry data or logic for planar calibration estimator. |
| `src/calibration/PlanarCalibrationEstimator.h` | Dichiara dati o logica geometrica per planar calibration estimator. | Declares geometry data or logic for planar calibration estimator. |

## Telecamere / Cameras

| File | IT | EN |
| --- | --- | --- |
| `src/camera/CameraDeviceInfo.h` | Dichiara gestione camera/acquisizione per camera device info. | Declares camera/acquisition handling for camera device info. |
| `src/camera/FileCamera.cpp` | Implementa gestione camera/acquisizione per file camera. | Implements camera/acquisition handling for file camera. |
| `src/camera/FileCamera.h` | Dichiara gestione camera/acquisizione per file camera. | Declares camera/acquisition handling for file camera. |
| `src/camera/ICamera.h` | Dichiara gestione camera/acquisizione per icamera. | Declares camera/acquisition handling for icamera. |
| `src/camera/SimulatedCamera.cpp` | Implementa gestione camera/acquisizione per simulated camera. | Implements camera/acquisition handling for simulated camera. |
| `src/camera/SimulatedCamera.h` | Dichiara gestione camera/acquisizione per simulated camera. | Declares camera/acquisition handling for simulated camera. |
| `src/camera/UsbCamera.cpp` | Implementa gestione camera/acquisizione per usb camera. | Implements camera/acquisition handling for usb camera. |
| `src/camera/UsbCamera.h` | Dichiara gestione camera/acquisizione per usb camera. | Declares camera/acquisition handling for usb camera. |
| `src/camera/UsbCameraDiscovery.cpp` | Implementa gestione camera/acquisizione per usb camera discovery. | Implements camera/acquisition handling for usb camera discovery. |
| `src/camera/UsbCameraDiscovery.h` | Dichiara gestione camera/acquisizione per usb camera discovery. | Declares camera/acquisition handling for usb camera discovery. |
| `src/camera/VimbaCamera.cpp` | Implementa gestione camera/acquisizione per vimba camera. | Implements camera/acquisition handling for vimba camera. |
| `src/camera/VimbaCamera.h` | Dichiara gestione camera/acquisizione per vimba camera. | Declares camera/acquisition handling for vimba camera. |
| `src/camera/VimbaTriggerController.cpp` | Implementa gestione camera/acquisizione per vimba trigger controller. | Implements camera/acquisition handling for vimba trigger controller. |
| `src/camera/VimbaTriggerController.h` | Dichiara gestione camera/acquisizione per vimba trigger controller. | Declares camera/acquisition handling for vimba trigger controller. |
| `src/camera/VimbaXDiscovery.cpp` | Implementa gestione camera/acquisizione per vimba xdiscovery. | Implements camera/acquisition handling for vimba xdiscovery. |
| `src/camera/VimbaXDiscovery.h` | Dichiara gestione camera/acquisizione per vimba xdiscovery. | Declares camera/acquisition handling for vimba xdiscovery. |

## Configurazione e ricette / Configuration and recipes

| File | IT | EN |
| --- | --- | --- |
| `src/config/AppConfig.cpp` | Implementa supporto per app config. | Implements support code for app config. |
| `src/config/AppConfig.h` | Dichiara supporto per app config. | Declares support code for app config. |
| `src/config/GeometryRecipeJson.cpp` | Implementa persistenza/configurazione per geometry recipe json. | Implements persistence/configuration logic for geometry recipe json. |
| `src/config/GeometryRecipeJson.h` | Dichiara persistenza/configurazione per geometry recipe json. | Declares persistence/configuration logic for geometry recipe json. |
| `src/config/RecipeDeletion.cpp` | Implementa persistenza/configurazione per recipe deletion. | Implements persistence/configuration logic for recipe deletion. |
| `src/config/RecipeDeletion.h` | Dichiara persistenza/configurazione per recipe deletion. | Declares persistence/configuration logic for recipe deletion. |
| `src/config/RecipeJsonUtils.cpp` | Implementa persistenza/configurazione per recipe json utils. | Implements persistence/configuration logic for recipe json utils. |
| `src/config/RecipeJsonUtils.h` | Dichiara persistenza/configurazione per recipe json utils. | Declares persistence/configuration logic for recipe json utils. |
| `src/config/RecipeManager.cpp` | Implementa caricamento e salvataggio ricette per recipe manager. | Implements recipe load/save logic for recipe manager. |
| `src/config/RecipeManager.h` | Dichiara caricamento e salvataggio ricette per recipe manager. | Declares recipe load/save logic for recipe manager. |
| `src/config/RecipeManagerAI.cpp` | Implementa caricamento e salvataggio ricette per recipe manager ai. | Implements recipe load/save logic for recipe manager ai. |
| `src/config/RecipeManagerGeometry.cpp` | Implementa caricamento e salvataggio ricette per recipe manager geometry. | Implements recipe load/save logic for recipe manager geometry. |
| `src/config/RecipeManagerMeasurement.cpp` | Implementa caricamento e salvataggio ricette per recipe manager measurement. | Implements recipe load/save logic for recipe manager measurement. |
| `src/config/RecipeManagerStorage.cpp` | Implementa caricamento e salvataggio ricette per recipe manager storage. | Implements recipe load/save logic for recipe manager storage. |
| `src/config/RecipeManagerSurfaceDefect.cpp` | Implementa caricamento e salvataggio ricette per recipe manager surface defect. | Implements recipe load/save logic for recipe manager surface defect. |
| `src/config/RecipeManagerSurfaceLocalization.cpp` | Implementa caricamento e salvataggio ricette per recipe manager surface localization. | Implements recipe load/save logic for recipe manager surface localization. |
| `src/config/RecipeManagerThreadInspection.cpp` | Implementa caricamento e salvataggio ricette per recipe manager thread inspection. | Implements recipe load/save logic for recipe manager thread inspection. |
| `src/config/TranslationManager.cpp` | Implementa supporto per translation manager. | Implements support code for translation manager. |
| `src/config/TranslationManager.h` | Dichiara supporto per translation manager. | Declares support code for translation manager. |

## Modelli geometrici / Geometry models

| File | IT | EN |
| --- | --- | --- |
| `src/geometry/ArcGeometry.cpp` | Implementa dati o logica geometrica per arc geometry. | Implements geometry data or logic for arc geometry. |
| `src/geometry/ArcGeometry.h` | Dichiara dati o logica geometrica per arc geometry. | Declares geometry data or logic for arc geometry. |
| `src/geometry/CircleGeometry.cpp` | Implementa dati o logica geometrica per circle geometry. | Implements geometry data or logic for circle geometry. |
| `src/geometry/CircleGeometry.h` | Dichiara dati o logica geometrica per circle geometry. | Declares geometry data or logic for circle geometry. |
| `src/geometry/ConstructedGeometry.cpp` | Implementa dati o logica geometrica per constructed geometry. | Implements geometry data or logic for constructed geometry. |
| `src/geometry/ConstructedGeometry.h` | Dichiara dati o logica geometrica per constructed geometry. | Declares geometry data or logic for constructed geometry. |
| `src/geometry/ConstructedGeometryMath.cpp` | Implementa dati o logica geometrica per constructed geometry math. | Implements geometry data or logic for constructed geometry math. |
| `src/geometry/ConstructedGeometryMath.h` | Dichiara dati o logica geometrica per constructed geometry math. | Declares geometry data or logic for constructed geometry math. |
| `src/geometry/ContourGeometry.cpp` | Implementa dati o logica geometrica per contour geometry. | Implements geometry data or logic for contour geometry. |
| `src/geometry/ContourGeometry.h` | Dichiara dati o logica geometrica per contour geometry. | Declares geometry data or logic for contour geometry. |
| `src/geometry/EdgeGeometry.cpp` | Implementa dati o logica geometrica per edge geometry. | Implements geometry data or logic for edge geometry. |
| `src/geometry/EdgeGeometry.h` | Dichiara dati o logica geometrica per edge geometry. | Declares geometry data or logic for edge geometry. |
| `src/geometry/GeometryCommon.h` | Dichiara dati o logica geometrica per geometry common. | Declares geometry data or logic for geometry common. |
| `src/geometry/GeometrySet.cpp` | Implementa dati o logica geometrica per geometry set. | Implements geometry data or logic for geometry set. |
| `src/geometry/GeometrySet.h` | Dichiara dati o logica geometrica per geometry set. | Declares geometry data or logic for geometry set. |
| `src/geometry/LineGeometry.cpp` | Implementa dati o logica geometrica per line geometry. | Implements geometry data or logic for line geometry. |
| `src/geometry/LineGeometry.h` | Dichiara dati o logica geometrica per line geometry. | Declares geometry data or logic for line geometry. |
| `src/geometry/PointGeometry.cpp` | Implementa dati o logica geometrica per point geometry. | Implements geometry data or logic for point geometry. |
| `src/geometry/PointGeometry.h` | Dichiara dati o logica geometrica per point geometry. | Declares geometry data or logic for point geometry. |

## GUI base / Base GUI

| File | IT | EN |
| --- | --- | --- |
| `src/gui/CameraAssignmentDialog.cpp` | Implementa il componente UI camera assignment dialog. | Implements the UI component camera assignment dialog. |
| `src/gui/CameraAssignmentDialog.h` | Dichiara il componente UI camera assignment dialog. | Declares the UI component camera assignment dialog. |
| `src/gui/CameraSetupPanelWidget.cpp` | Implementa il componente UI camera setup panel widget. | Implements the UI component camera setup panel widget. |
| `src/gui/CameraSetupPanelWidget.h` | Dichiara il componente UI camera setup panel widget. | Declares the UI component camera setup panel widget. |
| `src/gui/CameraStripWidget.cpp` | Implementa il componente UI camera strip widget. | Implements the UI component camera strip widget. |
| `src/gui/CameraStripWidget.h` | Dichiara il componente UI camera strip widget. | Declares the UI component camera strip widget. |
| `src/gui/CameraTileWidget.cpp` | Implementa il componente UI camera tile widget. | Implements the UI component camera tile widget. |
| `src/gui/CameraTileWidget.h` | Dichiara il componente UI camera tile widget. | Declares the UI component camera tile widget. |
| `src/gui/CheckerboardCalibrationDialog.cpp` | Implementa il componente UI checkerboard calibration dialog. | Implements the UI component checkerboard calibration dialog. |
| `src/gui/CheckerboardCalibrationDialog.h` | Dichiara il componente UI checkerboard calibration dialog. | Declares the UI component checkerboard calibration dialog. |
| `src/gui/CommandToolbarWidget.cpp` | Implementa il componente UI command toolbar widget. | Implements the UI component command toolbar widget. |
| `src/gui/CommandToolbarWidget.h` | Dichiara il componente UI command toolbar widget. | Declares the UI component command toolbar widget. |
| `src/gui/IconCatalog.cpp` | Implementa il componente UI icon catalog. | Implements the UI component icon catalog. |
| `src/gui/IconCatalog.h` | Dichiara il componente UI icon catalog. | Declares the UI component icon catalog. |
| `src/gui/ImagePrimitives.h` | Dichiara supporto per image primitives. | Declares support code for image primitives. |
| `src/gui/ImageViewWidget.cpp` | Implementa il componente UI image view widget. | Implements the UI component image view widget. |
| `src/gui/ImageViewWidget.h` | Dichiara il componente UI image view widget. | Declares the UI component image view widget. |
| `src/gui/ImageViewWidgetGeometry.cpp` | Implementa il componente UI image view widget geometry. | Implements the UI component image view widget geometry. |
| `src/gui/ImageViewWidgetInteraction.cpp` | Implementa il componente UI image view widget interaction. | Implements the UI component image view widget interaction. |
| `src/gui/ImageViewWidgetInteractionRelease.cpp` | Implementa il componente UI image view widget interaction release. | Implements the UI component image view widget interaction release. |
| `src/gui/ImageViewWidgetPainting.cpp` | Implementa il componente UI image view widget painting. | Implements the UI component image view widget painting. |
| `src/gui/ImageViewWidgetPolygon.cpp` | Implementa il componente UI image view widget polygon. | Implements the UI component image view widget polygon. |
| `src/gui/ImageViewWidgetState.cpp` | Implementa il componente UI image view widget state. | Implements the UI component image view widget state. |
| `src/gui/MainWindow.cpp` | Implementa supporto AI per main window. | Implements AI support for main window. |
| `src/gui/MainWindow.h` | Dichiara supporto AI per main window. | Declares AI support for main window. |
| `src/gui/MainWindowHelp.cpp` | Implementa supporto AI per main window help. | Implements AI support for main window help. |
| `src/gui/MainWindowImageHandlers.cpp` | Implementa supporto AI per main window image handlers. | Implements AI support for main window image handlers. |
| `src/gui/MainWindowInteractions.cpp` | Implementa supporto AI per main window interactions. | Implements AI support for main window interactions. |
| `src/gui/MainWindowMenu.cpp` | Implementa supporto AI per main window menu. | Implements AI support for main window menu. |
| `src/gui/MainWindowPanels.cpp` | Implementa i pannelli UI per main window panels. | Implements UI panels for main window panels. |
| `src/gui/MainWindowStyle.cpp` | Implementa supporto AI per main window style. | Implements AI support for main window style. |
| `src/gui/MainWindowSystem.cpp` | Implementa supporto AI per main window system. | Implements AI support for main window system. |
| `src/gui/MainWindowUi.cpp` | Implementa supporto AI per main window ui. | Implements AI support for main window ui. |
| `src/gui/MeasurementResultsWidget.cpp` | Implementa il componente UI measurement results widget. | Implements the UI component measurement results widget. |
| `src/gui/MeasurementResultsWidget.h` | Dichiara il componente UI measurement results widget. | Declares the UI component measurement results widget. |
| `src/gui/SurfaceLocalizationAdapters.cpp` | Implementa supporto per surface localization adapters. | Implements support code for surface localization adapters. |
| `src/gui/SurfaceLocalizationAdapters.h` | Dichiara supporto per surface localization adapters. | Declares support code for surface localization adapters. |
| `src/gui/SurfaceLocalizationPanelWidget.cpp` | Implementa il componente UI surface localization panel widget. | Implements the UI component surface localization panel widget. |
| `src/gui/SurfaceLocalizationPanelWidget.h` | Dichiara il componente UI surface localization panel widget. | Declares the UI component surface localization panel widget. |
| `src/gui/SurfaceLocalizationStrategies.cpp` | Implementa supporto per surface localization strategies. | Implements support code for surface localization strategies. |
| `src/gui/SurfaceLocalizationStrategies.h` | Dichiara supporto per surface localization strategies. | Declares support code for surface localization strategies. |
| `src/gui/ToolCatalog.cpp` | Implementa il componente UI tool catalog. | Implements the UI component tool catalog. |
| `src/gui/ToolCatalog.h` | Dichiara il componente UI tool catalog. | Declares the UI component tool catalog. |
| `src/gui/ToolIconBarWidget.cpp` | Implementa il componente UI tool icon bar widget. | Implements the UI component tool icon bar widget. |
| `src/gui/ToolIconBarWidget.h` | Dichiara il componente UI tool icon bar widget. | Declares the UI component tool icon bar widget. |
| `src/gui/ToolPanelWidget.cpp` | Implementa il componente UI tool panel widget. | Implements the UI component tool panel widget. |
| `src/gui/ToolPanelWidget.h` | Dichiara il componente UI tool panel widget. | Declares the UI component tool panel widget. |
| `src/gui/TouchIconButton.cpp` | Implementa il componente UI touch icon button. | Implements the UI component touch icon button. |
| `src/gui/TouchIconButton.h` | Dichiara il componente UI touch icon button. | Declares the UI component touch icon button. |
| `src/gui/UsbCameraAssignmentDialog.cpp` | Implementa il componente UI usb camera assignment dialog. | Implements the UI component usb camera assignment dialog. |
| `src/gui/UsbCameraAssignmentDialog.h` | Dichiara il componente UI usb camera assignment dialog. | Declares the UI component usb camera assignment dialog. |

## Helper GUI geometrie / GUI geometry helpers

| File | IT | EN |
| --- | --- | --- |
| `src/gui/geometry/ArcGuideMath.cpp` | Implementa dati o logica geometrica per arc guide math. | Implements geometry data or logic for arc guide math. |
| `src/gui/geometry/ArcGuideMath.h` | Dichiara dati o logica geometrica per arc guide math. | Declares geometry data or logic for arc guide math. |
| `src/gui/geometry/BandGeometryEditor.cpp` | Implementa dati o logica geometrica per band geometry editor. | Implements geometry data or logic for band geometry editor. |
| `src/gui/geometry/BandGeometryEditor.h` | Dichiara dati o logica geometrica per band geometry editor. | Declares geometry data or logic for band geometry editor. |
| `src/gui/geometry/ConfiguredGeometryDetector.cpp` | Implementa il detector o riconoscitore configured geometry detector. | Implements the detector/recognizer for configured geometry detector. |
| `src/gui/geometry/ConfiguredGeometryDetector.h` | Dichiara il detector o riconoscitore configured geometry detector. | Declares the detector/recognizer for configured geometry detector. |
| `src/gui/geometry/GeometryDiagnosticDrawing.cpp` | Implementa dati o logica geometrica per geometry diagnostic drawing. | Implements geometry data or logic for geometry diagnostic drawing. |
| `src/gui/geometry/GeometryDiagnosticDrawing.h` | Dichiara dati o logica geometrica per geometry diagnostic drawing. | Declares geometry data or logic for geometry diagnostic drawing. |
| `src/gui/geometry/GeometryDisplayNames.cpp` | Implementa dati o logica geometrica per geometry display names. | Implements geometry data or logic for geometry display names. |
| `src/gui/geometry/GeometryDisplayNames.h` | Dichiara dati o logica geometrica per geometry display names. | Declares geometry data or logic for geometry display names. |
| `src/gui/geometry/GeometryGuideRuntime.cpp` | Implementa dati o logica geometrica per geometry guide runtime. | Implements geometry data or logic for geometry guide runtime. |
| `src/gui/geometry/GeometryGuideRuntime.h` | Dichiara dati o logica geometrica per geometry guide runtime. | Declares geometry data or logic for geometry guide runtime. |
| `src/gui/geometry/GeometryMath.cpp` | Implementa dati o logica geometrica per geometry math. | Implements geometry data or logic for geometry math. |
| `src/gui/geometry/GeometryMath.h` | Dichiara dati o logica geometrica per geometry math. | Declares geometry data or logic for geometry math. |
| `src/gui/geometry/GeometryOverlay.cpp` | Implementa dati o logica geometrica per geometry overlay. | Implements geometry data or logic for geometry overlay. |
| `src/gui/geometry/GeometryOverlay.h` | Dichiara dati o logica geometrica per geometry overlay. | Declares geometry data or logic for geometry overlay. |
| `src/gui/geometry/GeometryOverlayPrimitives.cpp` | Implementa dati o logica geometrica per geometry overlay primitives. | Implements geometry data or logic for geometry overlay primitives. |
| `src/gui/geometry/GeometryOverlayPrimitives.h` | Dichiara dati o logica geometrica per geometry overlay primitives. | Declares geometry data or logic for geometry overlay primitives. |
| `src/gui/geometry/GeometryRecipePersistence.cpp` | Implementa persistenza/configurazione per geometry recipe persistence. | Implements persistence/configuration logic for geometry recipe persistence. |
| `src/gui/geometry/GeometryRecipePersistence.h` | Dichiara persistenza/configurazione per geometry recipe persistence. | Declares persistence/configuration logic for geometry recipe persistence. |
| `src/gui/geometry/GeometryRuntimeConfig.h` | Dichiara dati o logica geometrica per geometry runtime config. | Declares geometry data or logic for geometry runtime config. |
| `src/gui/geometry/LineGeometryEditor.cpp` | Implementa dati o logica geometrica per line geometry editor. | Implements geometry data or logic for line geometry editor. |
| `src/gui/geometry/LineGeometryEditor.h` | Dichiara dati o logica geometrica per line geometry editor. | Declares geometry data or logic for line geometry editor. |
| `src/gui/geometry/LineGeometryMouseController.cpp` | Implementa dati o logica geometrica per line geometry mouse controller. | Implements geometry data or logic for line geometry mouse controller. |
| `src/gui/geometry/LineGeometryMouseController.h` | Dichiara dati o logica geometrica per line geometry mouse controller. | Declares geometry data or logic for line geometry mouse controller. |

## Aiuto integrato / Built-in help

| File | IT | EN |
| --- | --- | --- |
| `src/gui/help/HelpAssistantService.cpp` | Implementa supporto per help assistant service. | Implements support code for help assistant service. |
| `src/gui/help/HelpAssistantService.h` | Dichiara supporto per help assistant service. | Declares support code for help assistant service. |
| `src/gui/help/HelpAssistantService_DirectReply.cpp` | Implementa supporto per help assistant service direct reply. | Implements support code for help assistant service direct reply. |
| `src/gui/help/HelpAssistantService_Retrieval.cpp` | Implementa supporto per help assistant service retrieval. | Implements support code for help assistant service retrieval. |
| `src/gui/help/HelpDialog.cpp` | Implementa il componente UI help dialog. | Implements the UI component help dialog. |
| `src/gui/help/HelpDialog.h` | Dichiara il componente UI help dialog. | Declares the UI component help dialog. |
| `src/gui/help/HelpModelBootstrapper.cpp` | Implementa supporto per help model bootstrapper. | Implements support code for help model bootstrapper. |
| `src/gui/help/HelpModelBootstrapper.h` | Dichiara supporto per help model bootstrapper. | Declares support code for help model bootstrapper. |

## Moduli GUI principali / Main GUI modules

| File | IT | EN |
| --- | --- | --- |
| `src/gui/modules/ConstructedGeometryCircleSource.cpp` | Implementa dati o logica geometrica per constructed geometry circle source. | Implements geometry data or logic for constructed geometry circle source. |
| `src/gui/modules/ConstructedGeometryCircleSource.h` | Dichiara dati o logica geometrica per constructed geometry circle source. | Declares geometry data or logic for constructed geometry circle source. |
| `src/gui/modules/ConstructedGeometryLineSource.cpp` | Implementa dati o logica geometrica per constructed geometry line source. | Implements geometry data or logic for constructed geometry line source. |
| `src/gui/modules/ConstructedGeometryLineSource.h` | Dichiara dati o logica geometrica per constructed geometry line source. | Declares geometry data or logic for constructed geometry line source. |
| `src/gui/modules/ConstructedGeometryPointSource.cpp` | Implementa dati o logica geometrica per constructed geometry point source. | Implements geometry data or logic for constructed geometry point source. |
| `src/gui/modules/ConstructedGeometryPointSource.h` | Dichiara dati o logica geometrica per constructed geometry point source. | Declares geometry data or logic for constructed geometry point source. |
| `src/gui/modules/MainWindowCameraConfigModule.cpp` | Implementa il modulo GUI main window camera config module. | Implements the GUI module for main window camera config module. |
| `src/gui/modules/MainWindowCameraConfigModule.h` | Dichiara il modulo GUI main window camera config module. | Declares the GUI module for main window camera config module. |
| `src/gui/modules/MainWindowCameraProfile.cpp` | Implementa gestione camera/acquisizione per main window camera profile. | Implements camera/acquisition handling for main window camera profile. |
| `src/gui/modules/MainWindowCameraProfile.h` | Dichiara gestione camera/acquisizione per main window camera profile. | Declares camera/acquisition handling for main window camera profile. |
| `src/gui/modules/MainWindowConstructedGeometryModule.cpp` | Implementa il modulo GUI main window constructed geometry module. | Implements the GUI module for main window constructed geometry module. |
| `src/gui/modules/MainWindowConstructedGeometryModule.h` | Dichiara il modulo GUI main window constructed geometry module. | Declares the GUI module for main window constructed geometry module. |
| `src/gui/modules/MainWindowConstructedGeometryPanels.cpp` | Implementa i pannelli UI per main window constructed geometry panels. | Implements UI panels for main window constructed geometry panels. |
| `src/gui/modules/MainWindowConstructedGeometryRecipe.cpp` | Implementa persistenza/configurazione per main window constructed geometry recipe. | Implements persistence/configuration logic for main window constructed geometry recipe. |
| `src/gui/modules/MainWindowContext.h` | Dichiara supporto AI per main window context. | Declares AI support for main window context. |
| `src/gui/modules/MainWindowGeometryModule.h` | Dichiara il modulo GUI main window geometry module. | Declares the GUI module for main window geometry module. |
| `src/gui/modules/MainWindowImagingModule.cpp` | Implementa il modulo GUI main window imaging module. | Implements the GUI module for main window imaging module. |
| `src/gui/modules/MainWindowImagingModule.h` | Dichiara il modulo GUI main window imaging module. | Declares the GUI module for main window imaging module. |
| `src/gui/modules/MainWindowLocalizationModule.cpp` | Implementa il modulo GUI main window localization module. | Implements the GUI module for main window localization module. |
| `src/gui/modules/MainWindowLocalizationModule.h` | Dichiara il modulo GUI main window localization module. | Declares the GUI module for main window localization module. |
| `src/gui/modules/MainWindowMeasurementActions.cpp` | Implementa dati o calcoli delle misure/tolleranze per main window measurement actions. | Implements measurement/tolerance data or calculations for main window measurement actions. |
| `src/gui/modules/MainWindowMeasurementModule.cpp` | Implementa il modulo GUI main window measurement module. | Implements the GUI module for main window measurement module. |
| `src/gui/modules/MainWindowMeasurementModule.h` | Dichiara il modulo GUI main window measurement module. | Declares the GUI module for main window measurement module. |
| `src/gui/modules/MainWindowMeasurementPanels.cpp` | Implementa i pannelli UI per main window measurement panels. | Implements UI panels for main window measurement panels. |
| `src/gui/modules/MainWindowMeasurementRecipe.cpp` | Implementa persistenza/configurazione per main window measurement recipe. | Implements persistence/configuration logic for main window measurement recipe. |
| `src/gui/modules/MainWindowModuleBase.h` | Dichiara il modulo GUI main window module base. | Declares the GUI module for main window module base. |
| `src/gui/modules/MainWindowRecipeModule.cpp` | Implementa il modulo GUI main window recipe module. | Implements the GUI module for main window recipe module. |
| `src/gui/modules/MainWindowRecipeModule.h` | Dichiara il modulo GUI main window recipe module. | Declares the GUI module for main window recipe module. |
| `src/gui/modules/MainWindowRecipeModule_Delete.cpp` | Implementa il modulo GUI main window recipe module delete. | Implements the GUI module for main window recipe module delete. |
| `src/gui/modules/MainWindowSetupModule.cpp` | Implementa il modulo GUI main window setup module. | Implements the GUI module for main window setup module. |
| `src/gui/modules/MainWindowSetupModule.h` | Dichiara il modulo GUI main window setup module. | Declares the GUI module for main window setup module. |
| `src/gui/modules/MainWindowSurfaceModule.h` | Dichiara il modulo GUI main window surface module. | Declares the GUI module for main window surface module. |
| `src/gui/modules/MainWindowThreadModule.cpp` | Implementa il modulo GUI main window thread module. | Implements the GUI module for main window thread module. |
| `src/gui/modules/MainWindowThreadModule.h` | Dichiara il modulo GUI main window thread module. | Declares the GUI module for main window thread module. |
| `src/gui/modules/QtCompat.h` | Dichiara supporto per qt compat. | Declares support code for qt compat. |

## Modulo geometrie / Geometry module

| File | IT | EN |
| --- | --- | --- |
| `src/gui/modules/geometry/GeometryPanelNavigation.h` | Dichiara dati o logica geometrica per geometry panel navigation. | Declares geometry data or logic for geometry panel navigation. |
| `src/gui/modules/geometry/GeometryRuntimeModule.cpp` | Implementa dati o logica geometrica per geometry runtime module. | Implements geometry data or logic for geometry runtime module. |
| `src/gui/modules/geometry/GeometryRuntimeModule_Configured.cpp` | Implementa dati o logica geometrica per geometry runtime module configured. | Implements geometry data or logic for geometry runtime module configured. |
| `src/gui/modules/geometry/GeometryRuntimeModule_Lines.cpp` | Implementa dati o logica geometrica per geometry runtime module lines. | Implements geometry data or logic for geometry runtime module lines. |
| `src/gui/modules/geometry/GeometryRuntimeModule_PointCircle.cpp` | Implementa dati o logica geometrica per geometry runtime module point circle. | Implements geometry data or logic for geometry runtime module point circle. |
| `src/gui/modules/geometry/GeometryToolPanels.cpp` | Implementa i pannelli UI per geometry tool panels. | Implements UI panels for geometry tool panels. |
| `src/gui/modules/geometry/GeometryToolPanels_Circle.cpp` | Implementa i pannelli UI per geometry tool panels circle. | Implements UI panels for geometry tool panels circle. |
| `src/gui/modules/geometry/GeometryToolPanels_Line.cpp` | Implementa i pannelli UI per geometry tool panels line. | Implements UI panels for geometry tool panels line. |
| `src/gui/modules/geometry/GeometryToolPanels_Point.cpp` | Implementa i pannelli UI per geometry tool panels point. | Implements UI panels for geometry tool panels point. |
| `src/gui/modules/geometry/MainWindowGeometryModule_ArcPanels.cpp` | Implementa il modulo GUI main window geometry module arc panels. | Implements the GUI module for main window geometry module arc panels. |
| `src/gui/modules/geometry/MainWindowGeometryModule_ArcRuntime.cpp` | Implementa il modulo GUI main window geometry module arc runtime. | Implements the GUI module for main window geometry module arc runtime. |
| `src/gui/modules/geometry/MainWindowGeometryModule_Delete.cpp` | Implementa il modulo GUI main window geometry module delete. | Implements the GUI module for main window geometry module delete. |
| `src/gui/modules/geometry/MainWindowGeometryModule_Recipe.cpp` | Implementa il modulo GUI main window geometry module recipe. | Implements the GUI module for main window geometry module recipe. |

## Modulo setup / Setup module

| File | IT | EN |
| --- | --- | --- |
| `src/gui/modules/setup/AiClassificationPaths.cpp` | Implementa supporto AI per ai classification paths. | Implements AI support for ai classification paths. |
| `src/gui/modules/setup/AiClassificationPaths.h` | Dichiara supporto AI per ai classification paths. | Declares AI support for ai classification paths. |
| `src/gui/modules/setup/AiLocalizationPaths.cpp` | Implementa supporto AI per ai localization paths. | Implements AI support for ai localization paths. |
| `src/gui/modules/setup/AiLocalizationPaths.h` | Dichiara supporto AI per ai localization paths. | Declares AI support for ai localization paths. |
| `src/gui/modules/setup/AiModelComparison.cpp` | Implementa supporto AI per ai model comparison. | Implements AI support for ai model comparison. |
| `src/gui/modules/setup/AiModelComparison.h` | Dichiara supporto AI per ai model comparison. | Declares AI support for ai model comparison. |
| `src/gui/modules/setup/AiPythonRuntime.cpp` | Implementa supporto AI per ai python runtime. | Implements AI support for ai python runtime. |
| `src/gui/modules/setup/AiPythonRuntime.h` | Dichiara supporto AI per ai python runtime. | Declares AI support for ai python runtime. |
| `src/gui/modules/setup/AiTrainingGraph.cpp` | Implementa supporto AI per ai training graph. | Implements AI support for ai training graph. |
| `src/gui/modules/setup/AiTrainingGraph.h` | Dichiara supporto AI per ai training graph. | Declares AI support for ai training graph. |
| `src/gui/modules/setup/MainWindowSetupModule_Acquisition.cpp` | Implementa il modulo GUI main window setup module acquisition. | Implements the GUI module for main window setup module acquisition. |
| `src/gui/modules/setup/MainWindowSetupModule_Ai.cpp` | Implementa il modulo GUI main window setup module ai. | Implements the GUI module for main window setup module ai. |
| `src/gui/modules/setup/MainWindowSetupModule_AiCapture.cpp` | Implementa il modulo GUI main window setup module ai capture. | Implements the GUI module for main window setup module ai capture. |
| `src/gui/modules/setup/MainWindowSetupModule_AiInference.cpp` | Implementa il modulo GUI main window setup module ai inference. | Implements the GUI module for main window setup module ai inference. |
| `src/gui/modules/setup/MainWindowSetupModule_AiLocalizationInference.cpp` | Implementa il modulo GUI main window setup module ai localization inference. | Implements the GUI module for main window setup module ai localization inference. |
| `src/gui/modules/setup/MainWindowSetupModule_AiLocalizationPanel.cpp` | Implementa il modulo GUI main window setup module ai localization panel. | Implements the GUI module for main window setup module ai localization panel. |
| `src/gui/modules/setup/MainWindowSetupModule_AiPanels.cpp` | Implementa il modulo GUI main window setup module ai panels. | Implements the GUI module for main window setup module ai panels. |
| `src/gui/modules/setup/MainWindowSetupModule_AiSegmentationPanels.cpp` | Implementa il modulo GUI main window setup module ai segmentation panels. | Implements the GUI module for main window setup module ai segmentation panels. |
| `src/gui/modules/setup/MainWindowSetupModule_AiTraining.cpp` | Implementa il modulo GUI main window setup module ai training. | Implements the GUI module for main window setup module ai training. |
| `src/gui/modules/setup/MainWindowSetupModule_Details.cpp` | Implementa il modulo GUI main window setup module details. | Implements the GUI module for main window setup module details. |
| `src/gui/modules/setup/MainWindowSetupModule_Runtime.cpp` | Implementa il modulo GUI main window setup module runtime. | Implements the GUI module for main window setup module runtime. |
| `src/gui/modules/setup/SetupCameraResolver.cpp` | Implementa gestione camera/acquisizione per setup camera resolver. | Implements camera/acquisition handling for setup camera resolver. |
| `src/gui/modules/setup/SetupCameraResolver.h` | Dichiara gestione camera/acquisizione per setup camera resolver. | Declares camera/acquisition handling for setup camera resolver. |
| `src/gui/modules/setup/SetupResultsDialog.cpp` | Implementa il componente UI setup results dialog. | Implements the UI component setup results dialog. |
| `src/gui/modules/setup/SetupResultsDialog.h` | Dichiara il componente UI setup results dialog. | Declares the UI component setup results dialog. |
| `src/gui/modules/setup/SetupResultsFormatter.cpp` | Implementa supporto per setup results formatter. | Implements support code for setup results formatter. |
| `src/gui/modules/setup/SetupResultsFormatter.h` | Dichiara supporto per setup results formatter. | Declares support code for setup results formatter. |

## Modulo superfici / Surface module

| File | IT | EN |
| --- | --- | --- |
| `src/gui/modules/surface/MainWindowSurfaceModule_LocalizationToolCircleAnnulus.cpp` | Implementa il modulo GUI main window surface module localization tool circle annulus. | Implements the GUI module for main window surface module localization tool circle annulus. |
| `src/gui/modules/surface/MainWindowSurfaceModule_LocalizationToolEdgePca.cpp` | Implementa il modulo GUI main window surface module localization tool edge pca. | Implements the GUI module for main window surface module localization tool edge pca. |
| `src/gui/modules/surface/MainWindowSurfaceModule_LocalizationToolMassPca.cpp` | Implementa il modulo GUI main window surface module localization tool mass pca. | Implements the GUI module for main window surface module localization tool mass pca. |
| `src/gui/modules/surface/MainWindowSurfaceModule_LocalizationToolModel.cpp` | Implementa il modulo GUI main window surface module localization tool model. | Implements the GUI module for main window surface module localization tool model. |
| `src/gui/modules/surface/MainWindowSurfaceModule_LocalizationToolPlanned.cpp` | Implementa il modulo GUI main window surface module localization tool planned. | Implements the GUI module for main window surface module localization tool planned. |
| `src/gui/modules/surface/MainWindowSurfaceModule_LocalizationToolTwoCircles.cpp` | Implementa il modulo GUI main window surface module localization tool two circles. | Implements the GUI module for main window surface module localization tool two circles. |
| `src/gui/modules/surface/MainWindowSurfaceModule_Model.cpp` | Implementa il modulo GUI main window surface module model. | Implements the GUI module for main window surface module model. |
| `src/gui/modules/surface/MainWindowSurfaceModule_Panels.cpp` | Implementa il modulo GUI main window surface module panels. | Implements the GUI module for main window surface module panels. |
| `src/gui/modules/surface/MainWindowSurfaceModule_Runtime.cpp` | Implementa il modulo GUI main window surface module runtime. | Implements the GUI module for main window surface module runtime. |

## Altro / Other

| File | IT | EN |
| --- | --- | --- |
| `src/main.cpp` | Implementa supporto AI per main. | Implements AI support for main. |

## Misure e tolleranze / Measurements and tolerances

| File | IT | EN |
| --- | --- | --- |
| `src/measurement/MeasurementGeometry.h` | Dichiara dati o logica geometrica per measurement geometry. | Declares geometry data or logic for measurement geometry. |
| `src/measurement/MeasurementGeometryMath.cpp` | Implementa dati o logica geometrica per measurement geometry math. | Implements geometry data or logic for measurement geometry math. |
| `src/measurement/MeasurementGeometryMath.h` | Dichiara dati o logica geometrica per measurement geometry math. | Declares geometry data or logic for measurement geometry math. |
| `src/measurement/MeasurementToleranceEvaluator.cpp` | Implementa dati o calcoli delle misure/tolleranze per measurement tolerance evaluator. | Implements measurement/tolerance data or calculations for measurement tolerance evaluator. |
| `src/measurement/MeasurementToleranceEvaluator.h` | Dichiara dati o calcoli delle misure/tolleranze per measurement tolerance evaluator. | Declares measurement/tolerance data or calculations for measurement tolerance evaluator. |
| `src/measurement/MeasurementUnitConverter.cpp` | Implementa dati o calcoli delle misure/tolleranze per measurement unit converter. | Implements measurement/tolerance data or calculations for measurement unit converter. |
| `src/measurement/MeasurementUnitConverter.h` | Dichiara dati o calcoli delle misure/tolleranze per measurement unit converter. | Declares measurement/tolerance data or calculations for measurement unit converter. |
| `src/measurement/RealMeasurementTypes.h` | Dichiara dati o calcoli delle misure/tolleranze per real measurement types. | Declares measurement/tolerance data or calculations for real measurement types. |

## Processing immagini / Image processing

| File | IT | EN |
| --- | --- | --- |
| `src/processing/CircleFit.cpp` | Implementa dati o logica geometrica per circle fit. | Implements geometry data or logic for circle fit. |
| `src/processing/CircleFit.h` | Dichiara dati o logica geometrica per circle fit. | Declares geometry data or logic for circle fit. |
| `src/processing/GeometryMeasurementPipeline.cpp` | Implementa la logica di elaborazione geometry measurement pipeline. | Implements processing logic for geometry measurement pipeline. |
| `src/processing/GeometryMeasurementPipeline.h` | Dichiara la logica di elaborazione geometry measurement pipeline. | Declares processing logic for geometry measurement pipeline. |
| `src/processing/ImageProcessor.cpp` | Implementa la logica di elaborazione image processor. | Implements processing logic for image processor. |
| `src/processing/ImageProcessor.h` | Dichiara la logica di elaborazione image processor. | Declares processing logic for image processor. |
| `src/processing/LocalizationProcessor.cpp` | Implementa la logica di elaborazione localization processor. | Implements processing logic for localization processor. |
| `src/processing/LocalizationProcessor.h` | Dichiara la logica di elaborazione localization processor. | Declares processing logic for localization processor. |
| `src/processing/MaskPoseEstimator.cpp` | Implementa supporto per mask pose estimator. | Implements support code for mask pose estimator. |
| `src/processing/MaskPoseEstimator.h` | Dichiara supporto per mask pose estimator. | Declares support code for mask pose estimator. |
| `src/processing/ShapeCandidateFilter.cpp` | Implementa supporto per shape candidate filter. | Implements support code for shape candidate filter. |
| `src/processing/ShapeCandidateFilter.h` | Dichiara supporto per shape candidate filter. | Declares support code for shape candidate filter. |
| `src/processing/SurfaceDefectProcessor.cpp` | Implementa la logica di elaborazione surface defect processor. | Implements processing logic for surface defect processor. |
| `src/processing/SurfaceDefectProcessor.h` | Dichiara la logica di elaborazione surface defect processor. | Declares processing logic for surface defect processor. |
| `src/processing/SurfaceEdgeStrategy.cpp` | Implementa la logica di elaborazione surface edge strategy. | Implements processing logic for surface edge strategy. |
| `src/processing/SurfaceEdgeStrategy.h` | Dichiara la logica di elaborazione surface edge strategy. | Declares processing logic for surface edge strategy. |
| `src/processing/SurfaceModelStrategy.cpp` | Implementa la logica di elaborazione surface model strategy. | Implements processing logic for surface model strategy. |
| `src/processing/SurfaceModelStrategy.h` | Dichiara la logica di elaborazione surface model strategy. | Declares processing logic for surface model strategy. |
| `src/processing/SurfaceModelTrainer.cpp` | Implementa supporto AI per surface model trainer. | Implements AI support for surface model trainer. |
| `src/processing/SurfaceModelTrainer.h` | Dichiara supporto AI per surface model trainer. | Declares AI support for surface model trainer. |
| `src/processing/SurfacePcaStrategy.cpp` | Implementa la logica di elaborazione surface pca strategy. | Implements processing logic for surface pca strategy. |
| `src/processing/SurfacePcaStrategy.h` | Dichiara la logica di elaborazione surface pca strategy. | Declares processing logic for surface pca strategy. |
| `src/processing/SurfaceProcessingUtils.h` | Dichiara supporto per surface processing utils. | Declares support code for surface processing utils. |
| `src/processing/SurfaceThresholdStrategy.cpp` | Implementa la logica di elaborazione surface threshold strategy. | Implements processing logic for surface threshold strategy. |
| `src/processing/SurfaceThresholdStrategy.h` | Dichiara la logica di elaborazione surface threshold strategy. | Declares processing logic for surface threshold strategy. |
| `src/processing/SurfaceTwoCirclesStrategy.cpp` | Implementa la logica di elaborazione surface two circles strategy. | Implements processing logic for surface two circles strategy. |
| `src/processing/SurfaceTwoCirclesStrategy.h` | Dichiara la logica di elaborazione surface two circles strategy. | Declares processing logic for surface two circles strategy. |

## Processing geometrie / Geometry processing

| File | IT | EN |
| --- | --- | --- |
| `src/processing/geometry/EdgeCircleDetector.cpp` | Implementa il detector o riconoscitore edge circle detector. | Implements the detector/recognizer for edge circle detector. |
| `src/processing/geometry/EdgeCircleDetector.h` | Dichiara il detector o riconoscitore edge circle detector. | Declares the detector/recognizer for edge circle detector. |
| `src/processing/geometry/EdgeLineDetector.cpp` | Implementa il detector o riconoscitore edge line detector. | Implements the detector/recognizer for edge line detector. |
| `src/processing/geometry/EdgeLineDetector.h` | Dichiara il detector o riconoscitore edge line detector. | Declares the detector/recognizer for edge line detector. |
| `src/processing/geometry/EdgePointDetector.cpp` | Implementa il detector o riconoscitore edge point detector. | Implements the detector/recognizer for edge point detector. |
| `src/processing/geometry/EdgePointDetector.h` | Dichiara il detector o riconoscitore edge point detector. | Declares the detector/recognizer for edge point detector. |
| `src/processing/geometry/EdgePointFilters.cpp` | Implementa dati o logica geometrica per edge point filters. | Implements geometry data or logic for edge point filters. |
| `src/processing/geometry/EdgePointFilters.h` | Dichiara dati o logica geometrica per edge point filters. | Declares geometry data or logic for edge point filters. |
| `src/processing/geometry/GeometryDetectorCommon.h` | Dichiara il detector o riconoscitore geometry detector common. | Declares the detector/recognizer for geometry detector common. |
| `src/processing/geometry/ThresholdCircleDetector.cpp` | Implementa il detector o riconoscitore threshold circle detector. | Implements the detector/recognizer for threshold circle detector. |
| `src/processing/geometry/ThresholdCircleDetector.h` | Dichiara il detector o riconoscitore threshold circle detector. | Declares the detector/recognizer for threshold circle detector. |
| `src/processing/geometry/ThresholdLineDetector.cpp` | Implementa il detector o riconoscitore threshold line detector. | Implements the detector/recognizer for threshold line detector. |
| `src/processing/geometry/ThresholdLineDetector.h` | Dichiara il detector o riconoscitore threshold line detector. | Declares the detector/recognizer for threshold line detector. |
| `src/processing/geometry/ThresholdMaskBuilder.cpp` | Implementa supporto per threshold mask builder. | Implements support code for threshold mask builder. |
| `src/processing/geometry/ThresholdMaskBuilder.h` | Dichiara supporto per threshold mask builder. | Declares support code for threshold mask builder. |

## Processing filetto / Thread processing

| File | IT | EN |
| --- | --- | --- |
| `src/processing/thread/ThreadInspectionUtils.h` | Dichiara logica del controllo filetto per thread inspection utils. | Declares thread-inspection logic for thread inspection utils. |
| `src/processing/thread/ThreadPhaseAnalyzer.cpp` | Implementa logica del controllo filetto per thread phase analyzer. | Implements thread-inspection logic for thread phase analyzer. |
| `src/processing/thread/ThreadPhaseAnalyzer.h` | Dichiara logica del controllo filetto per thread phase analyzer. | Declares thread-inspection logic for thread phase analyzer. |
| `src/processing/thread/ThreadProfileExtractor.cpp` | Implementa logica del controllo filetto per thread profile extractor. | Implements thread-inspection logic for thread profile extractor. |
| `src/processing/thread/ThreadProfileExtractor.h` | Dichiara logica del controllo filetto per thread profile extractor. | Declares thread-inspection logic for thread profile extractor. |
| `src/processing/thread/ThreadProfileMeasurer.cpp` | Implementa logica del controllo filetto per thread profile measurer. | Implements thread-inspection logic for thread profile measurer. |
| `src/processing/thread/ThreadProfileMeasurer.h` | Dichiara logica del controllo filetto per thread profile measurer. | Declares thread-inspection logic for thread profile measurer. |
| `src/processing/thread/ThreadProfileRootAnalyzer.cpp` | Implementa logica del controllo filetto per thread profile root analyzer. | Implements thread-inspection logic for thread profile root analyzer. |
| `src/processing/thread/ThreadProfileRootAnalyzer.h` | Dichiara logica del controllo filetto per thread profile root analyzer. | Declares thread-inspection logic for thread profile root analyzer. |

## Runtime

| File | IT | EN |
| --- | --- | --- |
| `src/runtime/CameraRuntime.cpp` | Implementa gestione camera/acquisizione per camera runtime. | Implements camera/acquisition handling for camera runtime. |
| `src/runtime/CameraRuntime.h` | Dichiara gestione camera/acquisizione per camera runtime. | Declares camera/acquisition handling for camera runtime. |
| `src/runtime/PartPose.h` | Dichiara supporto per part pose. | Declares support code for part pose. |

## Simulatore / Simulator

| File | IT | EN |
| --- | --- | --- |
| `src/simulator/SimulatorBridge.cpp` | Implementa supporto per simulator bridge. | Implements support code for simulator bridge. |
| `src/simulator/SimulatorBridge.h` | Dichiara supporto per simulator bridge. | Declares support code for simulator bridge. |
| `src/simulator/SimulatorProtocol.cpp` | Implementa supporto per simulator protocol. | Implements support code for simulator protocol. |
| `src/simulator/SimulatorProtocol.h` | Dichiara supporto per simulator protocol. | Declares support code for simulator protocol. |

## Utilita / Utilities

| File | IT | EN |
| --- | --- | --- |
| `src/util/AsyncExecutor.h` | Dichiara supporto per async executor. | Declares support code for async executor. |
| `src/util/CameraAsyncExecutor.cpp` | Implementa gestione camera/acquisizione per camera async executor. | Implements camera/acquisition handling for camera async executor. |
| `src/util/CameraAsyncExecutor.h` | Dichiara gestione camera/acquisizione per camera async executor. | Declares camera/acquisition handling for camera async executor. |
| `src/util/DetailedLogger.cpp` | Implementa supporto AI per detailed logger. | Implements AI support for detailed logger. |
| `src/util/DetailedLogger.h` | Dichiara supporto AI per detailed logger. | Declares AI support for detailed logger. |
| `src/util/ProductionThroughputTracker.cpp` | Implementa supporto per production throughput tracker. | Implements support code for production throughput tracker. |
| `src/util/ProductionThroughputTracker.h` | Dichiara supporto per production throughput tracker. | Declares support code for production throughput tracker. |
| `src/utils/Timer.h` | Dichiara supporto per timer. | Declares support code for timer. |
