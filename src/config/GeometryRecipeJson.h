#pragma once

#include <QJsonObject>
#include <QPointF>
#include <QString>

namespace GeometryRecipeJson
{
inline constexpr const char* kPartSpace = "part";
inline constexpr const char* kImageSpace = "image";

bool isImageSpace(const QString& coordinateSpace);
QString normalizedCoordinateSpace(const QString& coordinateSpace);

bool segmentRecipeValid(const QString& coordinateSpace,
                        const QPointF& partStart,
                        const QPointF& partEnd,
                        const QPointF& imageStart,
                        const QPointF& imageEnd);

bool centerRecipeValid(const QString& coordinateSpace,
                       const QPointF& partCenter,
                       const QPointF& imageCenter);

bool arcRecipeValid(const QString& coordinateSpace,
                    const QPointF& partCenter,
                    const QPointF& partStart,
                    const QPointF& partEnd,
                    const QPointF& imageCenter,
                    const QPointF& imageStart,
                    const QPointF& imageEnd);

void writeSegmentCoordinates(QJsonObject& object,
                           const QString& coordinateSpace,
                           const QPointF& partStart,
                           const QPointF& partEnd,
                           const QPointF& imageStart,
                           const QPointF& imageEnd);

void writeCenterCoordinate(QJsonObject& object,
                           const QString& coordinateSpace,
                           const QPointF& partCenter,
                           const QPointF& imageCenter);

void writeArcCoordinates(QJsonObject& object,
                         const QString& coordinateSpace,
                         const QPointF& partCenter,
                         const QPointF& partStart,
                         const QPointF& partEnd,
                         const QPointF& partThrough,
                         const QPointF& imageCenter,
                         const QPointF& imageStart,
                         const QPointF& imageEnd,
                         const QPointF& imageThrough);
}
