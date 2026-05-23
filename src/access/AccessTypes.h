#pragma once

#include <QString>

enum class AccessRole
{
  Viewer,
  Operator,
  Supervisor,
  Administrator,
  Guru
};

enum class AccessPermission
{
  ViewOverview,
  ViewCamera,
  StartStopMachine,
  EditSetup,
  EditGeometries,
  EditMeasurements,
  EditCalibration,
  EditRecipes,
  EditCameraSources,
  EditSystemSettings,
  ManageUsers
};

struct AccessUser
{
  QString id;
  QString displayName;
  AccessRole role = AccessRole::Viewer;
  bool enabled = true;
};

QString accessRoleId(AccessRole role);
QString accessRoleLabel(AccessRole role);
