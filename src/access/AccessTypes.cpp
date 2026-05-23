#include "access/AccessTypes.h"

QString accessRoleId(AccessRole role)
{
  switch (role)
  {
  case AccessRole::Operator:
    return "operator";
  case AccessRole::Supervisor:
    return "supervisor";
  case AccessRole::Administrator:
    return "administrator";
  case AccessRole::Guru:
    return "guru";
  case AccessRole::Viewer:
  default:
    return "viewer";
  }
}

QString accessRoleLabel(AccessRole role)
{
  switch (role)
  {
  case AccessRole::Operator:
    return "Operatore";
  case AccessRole::Supervisor:
    return "Supervisore";
  case AccessRole::Administrator:
    return "Admin";
  case AccessRole::Guru:
    return "Guru";
  case AccessRole::Viewer:
  default:
    return "Visualizzazione";
  }
}
