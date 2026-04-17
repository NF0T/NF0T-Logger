#pragma once

#include <QObject>
#include <QString>

#include "core/logbook/Qso.h"

/// Abstract base for digital mode listener services (WSJT-X, JS8Call, …).
///
/// Concrete services inherit this class.  MainWindow wires all listeners
/// generically through this interface — adding a new protocol requires no
/// changes to MainWindow.
///
/// Each service reads its own settings (enabled flag, port, address, etc.)
/// from Settings::instance() so start() requires no parameters.
class DigitalListenerService : public QObject
{
    Q_OBJECT

public:
    explicit DigitalListenerService(QObject *parent = nullptr) : QObject(parent) {}
    ~DigitalListenerService() override = default;

    virtual QString displayName() const = 0;

    /// Whether this service is enabled in Settings.
    virtual bool isEnabled() const = 0;

    virtual bool isRunning() const = 0;

    /// Start listening using current Settings values.
    /// Returns true on success; on failure emits logMessage() with details.
    virtual bool start() = 0;

    virtual void stop() = 0;

signals:
    /// Emitted when the selected DX station changes.  An empty dxCall means
    /// no station is currently selected (equivalent to a cancel/clear).
    void stationSelected(const QString &dxCall, const QString &dxGrid);

    /// Emitted when the radio's dial frequency or mode changes.
    /// Only relevant when no hardware radio backend is connected.
    void radioStatusChanged(quint64 dialFreqHz,
                            const QString &mode,
                            const QString &submode);

    /// Emitted when the digital mode software logs a completed contact.
    void qsoLogged(const Qso &qso);

    /// Emitted when the active QSO is cancelled in the digital mode software.
    void cleared();

    /// Human-readable status / log line (heartbeat, errors, etc.).
    void logMessage(const QString &msg);

    /// Lifecycle — emitted after the socket/connection is established.
    void started();

    /// Lifecycle — emitted after the socket/connection is torn down.
    void stopped();
};
