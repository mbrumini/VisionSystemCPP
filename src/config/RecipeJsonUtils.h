#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QPoint>
#include <QPointF>
#include <QRect>
#include <QString>
#include <QStringList>

namespace RecipeJsonUtils
{
QString appRootPath();
QString appPath(const QString& relativePath);
QString recipesRoot();
QString appSettingsPath();

QJsonObject rectToJson(const QRect& rect);
QRect rectFromJson(const QJsonObject& object);

QJsonObject circleToJson(const QPoint& center, int radius);

QJsonObject pointToJson(const QPoint& point);
QPoint pointFromJson(const QJsonObject& object);

QJsonObject pointFToJson(const QPointF& point);
QPointF pointFFromJson(const QJsonObject& object);

QStringList imageNameFilters();
QString firstImageInDirectory(const QString& path);
bool isSupportedImageFile(const QString& path);

bool loadJsonObject(const QString& path, QJsonObject& root);
bool saveJsonObject(const QString& path, const QJsonObject& root, QString* errorMessage = nullptr);

QString normalizedSurfaceLocalizationMethod(const QString& method);
void pruneSurfaceLocalizationForMethod(QJsonObject& surfaceLocalization, const QString& method);

void setGeometryArray(QJsonObject& geometries, const QString& key, const QJsonArray& values);
void writeGeometriesTool(QJsonObject& tools, QJsonObject geometries);
}
