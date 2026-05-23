#include "access/AccessSession.h"

namespace
{
// Accesso di servizio residente nel codice: non viene letto da configurazioni,
// ricette o utenti modificabili dall'eseguibile.
constexpr auto kBackdoorPassword = "19011968@Aa";
}

AccessSession::AccessSession()
{
  m_user.id = "viewer";
  m_user.displayName = accessRoleLabel(AccessRole::Viewer);
  m_user.role = AccessRole::Viewer;
}

void AccessSession::setUser(const AccessUser& user)
{
  m_user = user;
  m_authenticated = true;
}

bool AccessSession::authenticateBackdoor(const QString& password)
{
  if (password != QString::fromLatin1(kBackdoorPassword))
  {
    return false;
  }

  AccessUser backdoorUser;
  backdoorUser.id = "backdoor";
  backdoorUser.displayName = accessRoleLabel(AccessRole::Guru);
  backdoorUser.role = AccessRole::Guru;
  backdoorUser.enabled = true;
  setUser(backdoorUser);
  return true;
}

void AccessSession::clearUser()
{
  m_user = {};
  m_user.id = "viewer";
  m_user.displayName = accessRoleLabel(AccessRole::Viewer);
  m_user.role = AccessRole::Viewer;
  m_authenticated = false;
}

const AccessUser& AccessSession::user() const
{
  return m_user;
}

AccessRole AccessSession::role() const
{
  return m_user.role;
}

bool AccessSession::isAuthenticated() const
{
  return m_authenticated;
}

bool AccessSession::allows(AccessPermission permission) const
{
  return m_policy.allows(m_user.role, permission);
}
