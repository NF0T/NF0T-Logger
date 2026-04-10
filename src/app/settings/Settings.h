#pragma once

#include <optional>
#include <QObject>
#include <QString>
#include <QByteArray>

/// Typed settings singleton.
/// Non-sensitive values are stored in QSettings (INI file).
/// Sensitive values (passwords, API keys) are stored in the system keychain
/// via SecureSettings — call SecureSettings::instance().loadAll() at startup.
class Settings : public QObject
{
    Q_OBJECT

public:
    static Settings &instance();

    // -----------------------------------------------------------------------
    // Station (defaults applied to every new QSO)
    // -----------------------------------------------------------------------
    QString stationCallsign() const;
    void    setStationCallsign(const QString &v);

    QString stationName() const;
    void    setStationName(const QString &v);

    QString stationGridsquare() const;
    void    setStationGridsquare(const QString &v);

    QString stationCity() const;
    void    setStationCity(const QString &v);

    QString stationState() const;
    void    setStationState(const QString &v);

    QString stationCountry() const;
    void    setStationCountry(const QString &v);

    int     stationDxcc() const;
    void    setStationDxcc(int v);

    int     stationCqZone() const;
    void    setStationCqZone(int v);

    int     stationItuZone() const;
    void    setStationItuZone(int v);

    std::optional<double> stationLat() const;
    void    setStationLat(std::optional<double> v);

    std::optional<double> stationLon() const;
    void    setStationLon(std::optional<double> v);

    // -----------------------------------------------------------------------
    // Display preferences
    // -----------------------------------------------------------------------
    bool    useMetricUnits() const;       // false = imperial (miles), true = metric (km)
    void    setUseMetricUnits(bool v);

    // -----------------------------------------------------------------------
    // Equipment defaults
    // -----------------------------------------------------------------------
    QString equipmentRig() const;
    void    setEquipmentRig(const QString &v);

    QString equipmentAntenna() const;
    void    setEquipmentAntenna(const QString &v);

    double  equipmentTxPwr() const;
    void    setEquipmentTxPwr(double v);

    // -----------------------------------------------------------------------
    // Database
    // -----------------------------------------------------------------------
    QString dbBackend() const;          // "sqlite" | "mariadb"
    void    setDbBackend(const QString &v);

    QString dbSqlitePath() const;       // empty = use default location
    void    setDbSqlitePath(const QString &v);

    QString dbMariadbHost() const;
    void    setDbMariadbHost(const QString &v);

    int     dbMariadbPort() const;
    void    setDbMariadbPort(int v);

    QString dbMariadbDatabase() const;
    void    setDbMariadbDatabase(const QString &v);

    QString dbMariadbUsername() const;
    void    setDbMariadbUsername(const QString &v);

    // Secure — stored in system keychain
    QString dbMariadbPassword() const;
    void    setDbMariadbPassword(const QString &v);

    // -----------------------------------------------------------------------
    // Radio — Hamlib
    // -----------------------------------------------------------------------
    bool    hamlibEnabled() const;
    void    setHamlibEnabled(bool v);

    int     hamlibRigModel() const;
    void    setHamlibRigModel(int v);

    QString hamlibConnectionType() const;  // "serial" | "network"
    void    setHamlibConnectionType(const QString &v);

    QString hamlibSerialDevice() const;
    void    setHamlibSerialDevice(const QString &v);

    int     hamlibBaudRate() const;
    void    setHamlibBaudRate(int v);

    int     hamlibDataBits() const;
    void    setHamlibDataBits(int v);

    int     hamlibStopBits() const;
    void    setHamlibStopBits(int v);

    QString hamlibParity() const;       // "None" | "Even" | "Odd"
    void    setHamlibParity(const QString &v);

    QString hamlibFlowControl() const;  // "None" | "Hardware" | "Software"
    void    setHamlibFlowControl(const QString &v);

    QString hamlibNetworkHost() const;
    void    setHamlibNetworkHost(const QString &v);

    int     hamlibNetworkPort() const;
    void    setHamlibNetworkPort(int v);

    // -----------------------------------------------------------------------
    // Radio — TCI
    // -----------------------------------------------------------------------
    bool    tciEnabled() const;
    void    setTciEnabled(bool v);

    QString tciHost() const;
    void    setTciHost(const QString &v);

    int     tciPort() const;
    void    setTciPort(int v);

    // -----------------------------------------------------------------------
    // QSL — LoTW
    // -----------------------------------------------------------------------
    bool    lotwEnabled() const;
    void    setLotwEnabled(bool v);

    QString lotwTqslPath() const;
    void    setLotwTqslPath(const QString &v);

    QString lotwCallsign() const;
    void    setLotwCallsign(const QString &v);

    QString lotwStationLocation() const;
    void    setLotwStationLocation(const QString &v);

    // Secure
    QString lotwPassword() const;
    void    setLotwPassword(const QString &v);

    // -----------------------------------------------------------------------
    // QSL — eQSL
    // -----------------------------------------------------------------------
    bool    eqslEnabled() const;
    void    setEqslEnabled(bool v);

    QString eqslUsername() const;
    void    setEqslUsername(const QString &v);

    QString eqslNickname() const;
    void    setEqslNickname(const QString &v);

    // Secure
    QString eqslPassword() const;
    void    setEqslPassword(const QString &v);

    // -----------------------------------------------------------------------
    // QSL — QRZ
    // -----------------------------------------------------------------------
    bool    qrzEnabled() const;
    void    setQrzEnabled(bool v);

    QString qrzUsername() const;
    void    setQrzUsername(const QString &v);

    // Secure
    QString qrzApiKey() const;
    void    setQrzApiKey(const QString &v);

    // -----------------------------------------------------------------------
    // QSL — ClubLog
    // -----------------------------------------------------------------------
    bool    clublogEnabled() const;
    void    setClublogEnabled(bool v);

    QString clublogEmail() const;
    void    setClublogEmail(const QString &v);

    QString clublogCallsign() const;
    void    setClublogCallsign(const QString &v);

    // Secure
    QString clublogPassword() const;
    void    setClublogPassword(const QString &v);

    QString clublogAppKey() const;
    void    setClublogAppKey(const QString &v);

    // -----------------------------------------------------------------------
    // UI state (window geometry etc.)
    // -----------------------------------------------------------------------
    QByteArray mainWindowGeometry() const;
    void       setMainWindowGeometry(const QByteArray &v);

    QByteArray mainWindowState() const;
    void       setMainWindowState(const QByteArray &v);

    QByteArray splitterState() const;
    void       setSplitterState(const QByteArray &v);

signals:
    void changed();   // emitted after any value is written

private:
    Settings();

    template<typename T>
    T  get(const QString &key, const T &defaultValue) const;
    void put(const QString &key, const QVariant &value);
};
