#pragma once

#include <QObject>
#include <QString>

/// Abstract base for all CAT / radio control backends (Hamlib, TCI, …).
///
/// Concrete backends inherit this class and implement the pure virtuals.
/// MainWindow wires all backends generically through this interface, so
/// adding a new backend requires no changes to MainWindow.
class RadioBackend : public QObject
{
    Q_OBJECT

public:
    explicit RadioBackend(QObject *parent = nullptr) : QObject(parent) {}
    ~RadioBackend() override = default;

    virtual QString displayName() const = 0;
    virtual bool    isConnected()  const = 0;

    /// Open the connection using current Settings values.
    /// Returns true on success; on failure emits error() with details.
    virtual bool connectRadio() = 0;

    /// Gracefully close the connection.
    virtual void disconnectRadio() = 0;

public slots:
    virtual void setFreq(double freqMhz) = 0;
    virtual void setMode(const QString &adifMode, const QString &submode = {}) = 0;

signals:
    void freqChanged(double freqMhz);
    void modeChanged(const QString &mode, const QString &submode);
    void transmitChanged(bool transmitting);
    void connected();
    void disconnected();
    void error(const QString &message);
};
