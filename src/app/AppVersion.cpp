#include "app/AppVersion.h"

#include "AppVersion.generated.h"

QString AppVersion::displayString()
{
  return QStringLiteral(VISION_APP_VERSION_STRING);
}
