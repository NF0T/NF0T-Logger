#pragma once

#include "SettingsPage.h"

class QCheckBox;
class QLineEdit;
class QSpinBox;

class WsjtxPage : public SettingsPage
{
    Q_OBJECT

public:
    explicit WsjtxPage(QWidget *parent = nullptr);

    QString pageTitle() const override { return tr("WSJT-X"); }
    void load()  override;
    void apply() override;

private:
    QCheckBox *m_enabled        = nullptr;
    QSpinBox  *m_port           = nullptr;
    QLineEdit *m_multicastGroup = nullptr;
    QCheckBox *m_autoLog        = nullptr;
};
