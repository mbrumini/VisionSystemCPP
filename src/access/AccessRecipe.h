#pragma once

#include "access/AccessTypes.h"

#include <QVector>

class AccessRecipe
{
public:
  bool isUserConfigurationEditable() const;
  QVector<AccessUser> loadUsers(const QString& configPath) const;
  bool saveUsers(const QString& configPath, const QVector<AccessUser>& users, QString* errorMessage = nullptr) const;
};
