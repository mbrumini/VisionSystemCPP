#include "access/AccessPolicy.h"

bool AccessPolicy::allows(AccessRole role, AccessPermission permission) const
{
  if (permission == AccessPermission::ViewOverview || permission == AccessPermission::ViewCamera)
  {
    return true;
  }

  if (role == AccessRole::Guru || role == AccessRole::Administrator)
  {
    return true;
  }

  if (role == AccessRole::Supervisor)
  {
    return permission != AccessPermission::ManageUsers &&
      permission != AccessPermission::EditSystemSettings;
  }

  if (role == AccessRole::Operator)
  {
    return permission == AccessPermission::StartStopMachine;
  }

  return false;
}
