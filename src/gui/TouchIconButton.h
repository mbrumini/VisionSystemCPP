#pragma once

#include <QString>

class QPushButton;
class QWidget;

QPushButton* createTouchIconButton(const QString& iconId, const QString& label, QWidget* parent);
QString iconIdForActionKey(const QString& actionKey);
