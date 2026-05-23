#pragma once

#include "access/AccessTypes.h"

class AccessPolicy
{
public:
  bool allows(AccessRole role, AccessPermission permission) const;
};
