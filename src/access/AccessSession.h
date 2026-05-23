#pragma once

#include "access/AccessPolicy.h"

class AccessSession
{
public:
  AccessSession();

  void setUser(const AccessUser& user);
  bool authenticateBackdoor(const QString& password);
  void clearUser();
  const AccessUser& user() const;
  AccessRole role() const;
  bool isAuthenticated() const;
  bool allows(AccessPermission permission) const;

private:
  AccessUser m_user;
  bool m_authenticated = false;
  AccessPolicy m_policy;
};
