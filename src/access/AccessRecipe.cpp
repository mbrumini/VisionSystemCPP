#include "access/AccessRecipe.h"

bool AccessRecipe::isUserConfigurationEditable() const
{
  return true;
}

QVector<AccessUser> AccessRecipe::loadUsers(const QString& configPath) const
{
  Q_UNUSED(configPath);
  return {};
}

bool AccessRecipe::saveUsers(const QString& configPath, const QVector<AccessUser>& users, QString* errorMessage) const
{
  Q_UNUSED(configPath);
  Q_UNUSED(users);
  Q_UNUSED(errorMessage);
  return false;
}
