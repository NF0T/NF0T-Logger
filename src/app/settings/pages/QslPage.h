#pragma once

#include "SettingsPage.h"

class QCheckBox;
class QLineEdit;

class QslPage : public SettingsPage
{
    Q_OBJECT

public:
    explicit QslPage(QWidget *parent = nullptr);

    QString pageTitle() const override { return tr("QSL Services"); }
    void load()  override;
    void apply() override;

private:
    // LoTW
    QCheckBox *m_lotwEnabled          = nullptr;
    QLineEdit *m_tqslPath             = nullptr;
    QLineEdit *m_lotwCallsign         = nullptr;
    QLineEdit *m_lotwStationLocation  = nullptr;
    QLineEdit *m_lotwPassword         = nullptr;

    // eQSL
    QCheckBox *m_eqslEnabled   = nullptr;
    QLineEdit *m_eqslUsername  = nullptr;
    QLineEdit *m_eqslNickname  = nullptr;
    QLineEdit *m_eqslPassword  = nullptr;

    // QRZ
    QCheckBox *m_qrzEnabled    = nullptr;
    QLineEdit *m_qrzUsername   = nullptr;
    QLineEdit *m_qrzApiKey     = nullptr;

    // ClubLog
    QCheckBox *m_clublogEnabled  = nullptr;
    QLineEdit *m_clublogEmail    = nullptr;
    QLineEdit *m_clublogCallsign = nullptr;
    QLineEdit *m_clublogPassword = nullptr;
    QLineEdit *m_clublogAppKey   = nullptr;
};
