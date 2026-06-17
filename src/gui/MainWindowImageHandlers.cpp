#include "gui/MainWindow.h"

#include "gui/modules/MainWindowCameraProfile.h"
#include "gui/geometry/GeometryDiagnosticDrawing.h"
#include "gui/geometry/GeometryMath.h"

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cmath>

void MainWindow::setupLargeImageHandlers()
{
  m_largeImage->setRoiChangedHandler([this](const QRect& roi) {
    if (m_selectedCameraId.isEmpty())
    {
      return;
    }

    QString error;

    if (m_activeDrawingRecipe == MainWindowActiveDrawingRecipe::SurfaceDefects)
    {
      if (!m_recipeManager.saveSurfaceDefectRoi(m_selectedCameraId, roi, &error))
      {
        appendLog(error);
        return;
      }

      appendLog(QString("%1: %2 x=%3 y=%4 w=%5 h=%6")
                  .arg(trText("log.surfaceRoiSaved"))
                  .arg(m_selectedCameraId)
                  .arg(roi.x())
                  .arg(roi.y())
                  .arg(roi.width())
                  .arg(roi.height()));
      return;
    }

    if (m_activeDrawingRecipe == MainWindowActiveDrawingRecipe::Geometry)
    {
      deactivateImageDrawingTools();
      return;
    }

    if (!m_recipeManager.saveLocalizationRoi(m_selectedCameraId, roi, &error))
    {
      appendLog(error);
      return;
    }

    appendLog(QString("%1: %2 x=%3 y=%4 w=%5 h=%6")
                .arg(trText("log.localizationRoiSaved"))
                .arg(m_selectedCameraId)
                .arg(roi.x())
                .arg(roi.y())
                .arg(roi.width())
                .arg(roi.height()));
  });
  m_largeImage->setPolygonChangedHandler([this](const QVector<QPoint>& polygon) {
    if (m_selectedCameraId.isEmpty() || polygon.size() < 3)
    {
      return;
    }

    if (m_activeDrawingRecipe != MainWindowActiveDrawingRecipe::SurfaceDefects)
    {
      return;
    }

    QString error;
    if (!m_recipeManager.saveSurfaceDefectPolygon(m_selectedCameraId, polygon, &error))
    {
      appendLog(error);
      return;
    }

    appendLog(QString("%1: %2 points=%3")
                .arg(trText("actions.polygon"))
                .arg(m_selectedCameraId)
                .arg(polygon.size()));
  });
  m_largeImage->setExclusionRectAddedHandler([this](const QRect& rect) {
    if (m_selectedCameraId.isEmpty())
    {
      return;
    }

    QString error;

    if (m_activeDrawingRecipe == MainWindowActiveDrawingRecipe::SurfaceDefects)
    {
      if (!m_recipeManager.addSurfaceDefectExclusionRect(m_selectedCameraId, rect, &error))
      {
        appendLog(error);
        return;
      }

      appendLog(QString("%1: %2 x=%3 y=%4 w=%5 h=%6")
                  .arg(trText("log.surfaceExclusionSaved"))
                  .arg(m_selectedCameraId)
                  .arg(rect.x())
                  .arg(rect.y())
                  .arg(rect.width())
                  .arg(rect.height()));
      return;
    }

    if (!m_recipeManager.addLocalizationExclusionRect(m_selectedCameraId, rect, &error))
    {
      appendLog(error);
      return;
    }

    appendLog(QString("%1: %2 x=%3 y=%4 w=%5 h=%6")
                .arg(trText("log.localizationExclusionSaved"))
                .arg(m_selectedCameraId)
                .arg(rect.x())
                .arg(rect.y())
                .arg(rect.width())
                .arg(rect.height()));
  });
  m_largeImage->setExclusionRectsChangedHandler([this](const QVector<QRect>& rects) {
    if (m_selectedCameraId.isEmpty())
    {
      return;
    }

    QString error;

    if (m_activeDrawingRecipe == MainWindowActiveDrawingRecipe::SurfaceDefects)
    {
      if (!m_recipeManager.saveSurfaceDefectExclusionRects(m_selectedCameraId, rects, &error))
      {
        appendLog(error);
        return;
      }

      appendLog(QString("%1: %2 (%3)")
                  .arg(trText("log.surfaceExclusionsUpdated"))
                  .arg(m_selectedCameraId)
                  .arg(rects.size()));
      return;
    }

    if (!m_recipeManager.saveLocalizationExclusionRects(m_selectedCameraId, rects, &error))
    {
      appendLog(error);
      return;
    }

    appendLog(QString("%1: %2 (%3)")
                .arg(trText("log.localizationExclusionsUpdated"))
                .arg(m_selectedCameraId)
                .arg(rects.size()));
  });
  m_largeImage->setCircleChangedHandler([this](bool outerCircle, const ImageCircle& circle) {
    if (m_selectedCameraId.isEmpty())
    {
      return;
    }

    QString error;
    if (m_surface.circleTarget() == MainWindowSurfaceModule::CircleTarget::None)
    {
      const SurfaceAnnulusLocalizationConfig current = m_recipeManager.loadSurfaceAnnulusLocalization(m_selectedCameraId);
      const int innerRadius = qMax(1, circle.radius - current.edgeBandInner);
      const int outerRadius = circle.radius + current.edgeBandOuter;

      if (!m_recipeManager.saveSurfaceAnnulusCircle(m_selectedCameraId, true, circle.center, outerRadius, &error) ||
          !m_recipeManager.saveSurfaceAnnulusCircle(m_selectedCameraId, false, circle.center, innerRadius, &error) ||
          !m_recipeManager.saveSurfaceEdgeCircle(m_selectedCameraId, circle.center, circle.radius, &error))
      {
        appendLog(error);
        return;
      }

      m_largeImage->setCircles({
        {circle.center, outerRadius},
        {circle.center, innerRadius}
      });

      appendLog(QString("%1: %2 cx=%3 cy=%4 r=%5")
                  .arg(trText("log.surfaceThreePointCircleSaved"))
                  .arg(m_selectedCameraId)
                  .arg(circle.center.x())
                  .arg(circle.center.y())
                  .arg(circle.radius));

      const SurfaceAnnulusLocalizationConfig annulus = m_recipeManager.loadSurfaceAnnulusLocalization(m_selectedCameraId);
      if ((annulus.method == "edge" && annulus.hasEdgeCircle && annulus.edgeRadius > annulus.edgeBandInner) ||
          (annulus.method != "edge" && annulus.hasOuterCircle && annulus.hasInnerCircle && annulus.outerRadius > annulus.innerRadius))
      {
        m_surface.testSurfaceAnnulusLocalization(m_selectedCamera);
      }
      return;
    }

    if (m_surface.circleTarget() == MainWindowSurfaceModule::CircleTarget::Edge)
    {
      if (!m_recipeManager.saveSurfaceEdgeCircle(m_selectedCameraId, circle.center, circle.radius, &error))
      {
        appendLog(error);
        return;
      }

      const SurfaceAnnulusLocalizationConfig annulus = m_recipeManager.loadSurfaceAnnulusLocalization(m_selectedCameraId);
      m_largeImage->setCircles({
        {circle.center, circle.radius + annulus.edgeBandOuter},
        {circle.center, qMax(1, circle.radius - annulus.edgeBandInner)}
      });

      appendLog(QString("%1: %2 cx=%3 cy=%4 r=%5")
                  .arg(trText("log.surfaceEdgeCircleSaved"))
                  .arg(m_selectedCameraId)
                  .arg(circle.center.x())
                  .arg(circle.center.y())
                  .arg(circle.radius));
      m_surface.testSurfaceAnnulusLocalization(m_selectedCamera);
      return;
    }

    const bool targetOuter = m_surface.circleTarget() == MainWindowSurfaceModule::CircleTarget::Inner ? false : outerCircle;

    if (!m_recipeManager.saveSurfaceAnnulusCircle(m_selectedCameraId, targetOuter, circle.center, circle.radius, &error))
    {
      appendLog(error);
      return;
    }

    const SurfaceAnnulusLocalizationConfig annulus = m_recipeManager.loadSurfaceAnnulusLocalization(m_selectedCameraId);
    QVector<ImageCircle> circles;
    if (annulus.hasOuterCircle)
    {
      circles.append({annulus.center, annulus.outerRadius});
    }
    if (annulus.hasInnerCircle)
    {
      circles.append({annulus.center, annulus.innerRadius});
    }
    m_largeImage->setCircles(circles);

    appendLog(QString("%1: %2 cx=%3 cy=%4 r=%5")
                .arg(targetOuter ? trText("log.surfaceOuterCircleSaved") : trText("log.surfaceInnerCircleSaved"))
                .arg(m_selectedCameraId)
                .arg(circle.center.x())
                .arg(circle.center.y())
                .arg(circle.radius));
  });
  m_largeImage->setThreePointCircleHandler([this](const QVector<QPoint>& points) {
    if (m_selectedCameraId.isEmpty())
    {
      return;
    }

    if (m_activeDrawingRecipe == MainWindowActiveDrawingRecipe::Geometry && m_geometry.drawingTarget() == MainWindowGeometryModule::DrawingTarget::Circle)
    {
      m_geometry.handleGeometryCirclePoints(m_selectedCamera, points);
      return;
    }

    if (m_activeDrawingRecipe == MainWindowActiveDrawingRecipe::Geometry && m_geometry.drawingTarget() == MainWindowGeometryModule::DrawingTarget::Arc)
    {
      m_geometry.handleGeometryArcPoints(m_selectedCamera, points);
      return;
    }

    ImageCircle circle;

    if (!GeometryMath::circleFromThreePoints(points, circle))
    {
      appendLog(trText("log.surfaceEdgeCircleInvalid") + ": " + m_selectedCameraId);
      return;
    }

    QString error;

    if (m_surface.circleTarget() == MainWindowSurfaceModule::CircleTarget::None)
    {
      const SurfaceAnnulusLocalizationConfig current = m_recipeManager.loadSurfaceAnnulusLocalization(m_selectedCameraId);
      const int innerRadius = qMax(1, circle.radius - current.edgeBandInner);
      const int outerRadius = circle.radius + current.edgeBandOuter;

      if (!m_recipeManager.saveSurfaceAnnulusCircle(m_selectedCameraId, true, circle.center, outerRadius, &error) ||
          !m_recipeManager.saveSurfaceAnnulusCircle(m_selectedCameraId, false, circle.center, innerRadius, &error) ||
          !m_recipeManager.saveSurfaceEdgeCircle(m_selectedCameraId, circle.center, circle.radius, &error))
      {
        appendLog(error);
        return;
      }

      m_largeImage->setCircles({
        {circle.center, outerRadius},
        {circle.center, innerRadius}
      });

      appendLog(QString("%1: %2 cx=%3 cy=%4 r=%5")
                  .arg(trText("log.surfaceThreePointCircleSaved"))
                  .arg(m_selectedCameraId)
                  .arg(circle.center.x())
                  .arg(circle.center.y())
                  .arg(circle.radius));

      const SurfaceAnnulusLocalizationConfig annulus = m_recipeManager.loadSurfaceAnnulusLocalization(m_selectedCameraId);
      if ((annulus.method == "edge" && annulus.hasEdgeCircle && annulus.edgeRadius > annulus.edgeBandInner) ||
          (annulus.method != "edge" && annulus.hasOuterCircle && annulus.hasInnerCircle && annulus.outerRadius > annulus.innerRadius))
      {
        m_surface.testSurfaceAnnulusLocalization(m_selectedCamera);
      }
      return;
    }

    if (m_surface.circleTarget() == MainWindowSurfaceModule::CircleTarget::Edge)
    {
      if (!m_recipeManager.saveSurfaceEdgeCircle(m_selectedCameraId, circle.center, circle.radius, &error))
      {
        appendLog(error);
        return;
      }

      const SurfaceAnnulusLocalizationConfig annulus = m_recipeManager.loadSurfaceAnnulusLocalization(m_selectedCameraId);
      m_largeImage->setCircles({
        {circle.center, circle.radius + annulus.edgeBandOuter},
        {circle.center, qMax(1, circle.radius - annulus.edgeBandInner)}
      });

      appendLog(QString("%1: %2 cx=%3 cy=%4 r=%5")
                  .arg(trText("log.surfaceEdgeCircleSaved"))
                  .arg(m_selectedCameraId)
                  .arg(circle.center.x())
                  .arg(circle.center.y())
                  .arg(circle.radius));
      m_surface.testSurfaceAnnulusLocalization(m_selectedCamera);
      return;
    }

    const bool targetOuter = m_surface.circleTarget() != MainWindowSurfaceModule::CircleTarget::Inner;

    if (!m_recipeManager.saveSurfaceAnnulusCircle(m_selectedCameraId, targetOuter, circle.center, circle.radius, &error))
    {
      appendLog(error);
      return;
    }

    const SurfaceAnnulusLocalizationConfig annulus = m_recipeManager.loadSurfaceAnnulusLocalization(m_selectedCameraId);
    QVector<ImageCircle> circles;
    if (annulus.hasOuterCircle)
    {
      circles.append({annulus.center, annulus.outerRadius});
    }
    if (annulus.hasInnerCircle)
    {
      circles.append({annulus.center, annulus.innerRadius});
    }
    m_largeImage->setCircles(circles);

    appendLog(QString("%1: %2 cx=%3 cy=%4 r=%5")
                .arg(targetOuter ? trText("log.surfaceOuterCircleSaved") : trText("log.surfaceInnerCircleSaved"))
                .arg(m_selectedCameraId)
                .arg(circle.center.x())
                .arg(circle.center.y())
                .arg(circle.radius));
  });
  m_largeImage->setTwoPointLineHandler([this](const QVector<QPoint>& points) {
    if (m_selectedCameraId.isEmpty() ||
        m_activeDrawingRecipe != MainWindowActiveDrawingRecipe::Geometry ||
        m_geometry.drawingTarget() != MainWindowGeometryModule::DrawingTarget::Line ||
        points.size() < 2)
    {
      return;
    }

    const PartPose& pose = m_cameraRuntime[m_selectedCameraId].currentPose();
    if (!pose.valid)
    {
      appendLog(trText("log.partPoseMissing") + ": " + m_selectedCameraId);
      deactivateImageDrawingTools();
      return;
    }

    GeometryLineRuntimeConfig& config = m_geometry.activeGeometryLineConfig(m_selectedCameraId);
    config.partStart = imageToPart(pose, cv::Point2d(points[0].x(), points[0].y()));
    config.partEnd = imageToPart(pose, cv::Point2d(points[1].x(), points[1].y()));
    config.hasLine = true;

    const cv::Point2d imageStart(points[0].x(), points[0].y());
    const cv::Point2d imageEnd(points[1].x(), points[1].y());
    const cv::Point2d center = (imageStart + imageEnd) * 0.5;
    const double length = std::hypot(imageEnd.x - imageStart.x, imageEnd.y - imageStart.y);
    const double angle = std::atan2(imageEnd.y - imageStart.y, imageEnd.x - imageStart.x) * 180.0 / CV_PI;
    m_largeImage->setGeometryArea({
      QPointF(center.x, center.y),
      QSizeF(length, config.bandHalfWidth * 2.0),
      angle
    });
    m_largeImage->setGeometryPoints({
      QPointF(imageStart.x, imageStart.y),
      QPointF(imageEnd.x, imageEnd.y)
    });
    m_largeImage->setGeometryLines({
      {QPointF(imageStart.x, imageStart.y), QPointF(imageEnd.x, imageEnd.y)}
    });
    m_geometry.setDrawingTarget(MainWindowGeometryModule::DrawingTarget::Line);
    m_largeImage->setGeometryAreaEditingEnabled(true);

    m_geometry.testGeometryLine(m_selectedCamera);
  });
  m_largeImage->setGeometryPointPickedHandler([this](const QPointF& imagePoint) {
    if (m_selectedCameraId.isEmpty() ||
        m_activeDrawingRecipe != MainWindowActiveDrawingRecipe::Geometry)
    {
      return;
    }

    if (m_geometry.drawingTarget() == MainWindowGeometryModule::DrawingTarget::Line)
    {
      m_geometry.handleGeometryLinePoint(m_selectedCamera, imagePoint);
    }
    else if (m_geometry.drawingTarget() == MainWindowGeometryModule::DrawingTarget::Point)
    {
      m_geometry.handleGeometryPointGuidePoint(m_selectedCamera, imagePoint);
    }
  });
  m_largeImage->setGeometryPointMovedHandler([this](const QPointF& imagePoint) {
    if (m_selectedCameraId.isEmpty() ||
        m_activeDrawingRecipe != MainWindowActiveDrawingRecipe::Geometry)
    {
      return;
    }

    if (m_geometry.drawingTarget() == MainWindowGeometryModule::DrawingTarget::Line)
    {
      LineGeometryMouseController& controller = m_geometry.lineMouseController(m_selectedCameraId);
      controller.handleMove(imagePoint);
      m_geometry.updateGeometryLineOverlay(m_selectedCamera);
    }
    else if (m_geometry.drawingTarget() == MainWindowGeometryModule::DrawingTarget::Point)
    {
      LineGeometryMouseController& controller = m_geometry.pointMouseController(m_selectedCameraId);
      controller.handleMove(imagePoint);
      m_geometry.updateGeometryPointOverlay(m_selectedCamera);
    }
  });
  m_largeImage->setGeometryOverlayPointMovedHandler([this](int pointIndex, const QPointF& imagePoint) {
    if (m_selectedCameraId.isEmpty())
    {
      return;
    }

    if (m_geometry.drawingTarget() == MainWindowGeometryModule::DrawingTarget::Line)
    {
      m_geometry.handleGeometryLineHandleMoved(m_selectedCamera, pointIndex, imagePoint);
    }
    else if (m_geometry.drawingTarget() == MainWindowGeometryModule::DrawingTarget::Point)
    {
      m_geometry.handleGeometryPointHandleMoved(m_selectedCamera, pointIndex, imagePoint);
    }
    else if (m_geometry.drawingTarget() == MainWindowGeometryModule::DrawingTarget::Arc)
    {
      m_geometry.handleGeometryArcHandleMoved(m_selectedCamera, pointIndex, imagePoint);
    }
  });
  m_largeImage->setGeometryOverlayDimensionLabelMovedHandler([this](const QString& measurementKey, const QPointF& imagePoint) {
    if (m_selectedCameraId.isEmpty())
    {
      return;
    }

    m_measurement.setMeasurementLabelPosition(m_selectedCamera, measurementKey, imagePoint);
  });
  m_largeImage->setGeometryAreaChangedHandler([this](const ImageRotatedRect& area) {
    if (m_selectedCameraId.isEmpty() || m_geometry.drawingTarget() != MainWindowGeometryModule::DrawingTarget::Line)
    {
      return;
    }

    const PartPose& pose = m_cameraRuntime[m_selectedCameraId].currentPose();
    if (!pose.valid)
    {
      return;
    }

    const double angle = area.angleDegrees * CV_PI / 180.0;
    const cv::Point2d axis(std::cos(angle), std::sin(angle));
    const cv::Point2d center(area.center.x(), area.center.y());
    GeometryLineRuntimeConfig& config = m_geometry.activeGeometryLineConfig(m_selectedCameraId);
    config.partStart = imageToPart(pose, center - axis * (area.size.width() * 0.5));
    config.partEnd = imageToPart(pose, center + axis * (area.size.width() * 0.5));
    config.bandHalfWidth = std::max(2, static_cast<int>(std::round(area.size.height() * 0.5)));
    config.hasLine = true;
    m_geometry.testGeometryLine(m_selectedCamera);
    m_geometry.showGeometryLinePanel(m_selectedCamera);
  });

}
