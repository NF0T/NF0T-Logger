#pragma once

#include <QDialog>
#include <QList>
#include <QMetaObject>
#include <QStringList>

#include "core/logbook/Qso.h"

class QComboBox;
class QLabel;
class QPlainTextEdit;
class QPushButton;
class QslService;

/// Non-modal dialog for uploading QSOs to a selected QSL service.
///
/// The user picks a service and clicks Upload. The service filters internally
/// to only unsent QSOs. Progress and server responses appear in the log area.
///
/// On completion, uploadCompleted() is emitted so the caller can persist the
/// updated QSOs (with sent flag + date set) to the database.
class QslUploadDialog : public QDialog
{
    Q_OBJECT

public:
    QslUploadDialog(const QList<QslService*> &services,
                    const QList<Qso>         &allQsos,
                    QWidget                  *parent = nullptr);

signals:
    /// Emitted when an upload operation finishes. updated QSOs have the
    /// sent flag and date already set by the service.
    void uploadCompleted(const QList<Qso> &updated, const QStringList &errors);

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onServiceChanged(int index);
    void onUploadClicked();
    void onUploadFinished(const QList<Qso> &updated, const QStringList &errors);

private:
    void appendLog(const QString &msg);
    void setRunning(bool running);
    void disconnectService();

    QList<QslService*>             m_services;
    QList<Qso>                     m_allQsos;
    QslService                    *m_currentService = nullptr;
    bool                           m_running        = false;
    QList<QMetaObject::Connection> m_serviceConns;

    QComboBox      *m_serviceCombo = nullptr;
    QLabel         *m_pendingLabel = nullptr;
    QPlainTextEdit *m_log          = nullptr;
    QLabel         *m_statusLabel  = nullptr;
    QPushButton    *m_uploadBtn    = nullptr;
    QPushButton    *m_closeBtn     = nullptr;
};
