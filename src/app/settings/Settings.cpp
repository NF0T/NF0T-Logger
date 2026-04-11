#include "Settings.h"
#include "SecureSettings.h"

#include <QDate>
#include <QSettings>

// ---------------------------------------------------------------------------
// Singleton + helpers
// ---------------------------------------------------------------------------

Settings::Settings() = default;

Settings &Settings::instance()
{
    static Settings s;
    return s;
}

template<typename T>
T Settings::get(const QString &key, const T &defaultValue) const
{
    QSettings qs;
    return qs.value(key, defaultValue).template value<T>();
}

void Settings::put(const QString &key, const QVariant &value)
{
    QSettings qs;
    qs.setValue(key, value);
    emit changed();
}

// ---------------------------------------------------------------------------
// Station
// ---------------------------------------------------------------------------

QString Settings::stationCallsign() const    { return get("station/callsign", QString()); }
void Settings::setStationCallsign(const QString &v) { put("station/callsign", v); }

QString Settings::stationName() const        { return get("station/name", QString()); }
void Settings::setStationName(const QString &v)     { put("station/name", v); }

QString Settings::stationGridsquare() const  { return get("station/gridsquare", QString()); }
void Settings::setStationGridsquare(const QString &v) { put("station/gridsquare", v); }

QString Settings::stationCity() const        { return get("station/city", QString()); }
void Settings::setStationCity(const QString &v)     { put("station/city", v); }

QString Settings::stationState() const       { return get("station/state", QString()); }
void Settings::setStationState(const QString &v)    { put("station/state", v); }

QString Settings::stationCountry() const     { return get("station/country", QString()); }
void Settings::setStationCountry(const QString &v)  { put("station/country", v); }

int Settings::stationDxcc() const            { return get("station/dxcc", 0); }
void Settings::setStationDxcc(int v)         { put("station/dxcc", v); }

int Settings::stationCqZone() const          { return get("station/cqzone", 0); }
void Settings::setStationCqZone(int v)       { put("station/cqzone", v); }

int Settings::stationItuZone() const         { return get("station/ituzone", 0); }
void Settings::setStationItuZone(int v)      { put("station/ituzone", v); }

std::optional<double> Settings::stationLat() const
{
    QSettings qs;
    const QVariant v = qs.value("station/lat");
    return v.isNull() ? std::nullopt : std::optional<double>(v.toDouble());
}
void Settings::setStationLat(std::optional<double> v)
{
    QSettings qs;
    if (v) qs.setValue("station/lat", *v); else qs.remove("station/lat");
    emit changed();
}

std::optional<double> Settings::stationLon() const
{
    QSettings qs;
    const QVariant v = qs.value("station/lon");
    return v.isNull() ? std::nullopt : std::optional<double>(v.toDouble());
}
void Settings::setStationLon(std::optional<double> v)
{
    QSettings qs;
    if (v) qs.setValue("station/lon", *v); else qs.remove("station/lon");
    emit changed();
}

// ---------------------------------------------------------------------------
// Display preferences
// ---------------------------------------------------------------------------

bool Settings::useMetricUnits() const       { return get("display/metric_units", false); }
void Settings::setUseMetricUnits(bool v)    { put("display/metric_units", v); }

// Equipment
// ---------------------------------------------------------------------------

QString Settings::equipmentRig() const      { return get("equipment/rig", QString()); }
void Settings::setEquipmentRig(const QString &v)    { put("equipment/rig", v); }

QString Settings::equipmentAntenna() const  { return get("equipment/antenna", QString()); }
void Settings::setEquipmentAntenna(const QString &v){ put("equipment/antenna", v); }

double Settings::equipmentTxPwr() const     { return get("equipment/txpwr", 0.0); }
void Settings::setEquipmentTxPwr(double v)  { put("equipment/txpwr", v); }

// ---------------------------------------------------------------------------
// Database
// ---------------------------------------------------------------------------

QString Settings::dbBackend() const         { return get("db/backend", QStringLiteral("sqlite")); }
void Settings::setDbBackend(const QString &v)       { put("db/backend", v); }

QString Settings::dbSqlitePath() const      { return get("db/sqlite/path", QString()); }
void Settings::setDbSqlitePath(const QString &v)    { put("db/sqlite/path", v); }

QString Settings::dbMariadbHost() const     { return get("db/mariadb/host", QStringLiteral("127.0.0.1")); }
void Settings::setDbMariadbHost(const QString &v)   { put("db/mariadb/host", v); }

int Settings::dbMariadbPort() const         { return get("db/mariadb/port", 3306); }
void Settings::setDbMariadbPort(int v)      { put("db/mariadb/port", v); }

QString Settings::dbMariadbDatabase() const { return get("db/mariadb/database", QStringLiteral("nf0t_logger")); }
void Settings::setDbMariadbDatabase(const QString &v){ put("db/mariadb/database", v); }

QString Settings::dbMariadbUsername() const { return get("db/mariadb/username", QString()); }
void Settings::setDbMariadbUsername(const QString &v){ put("db/mariadb/username", v); }

QString Settings::dbMariadbPassword() const { return SecureSettings::instance().get(SecureKey::DB_MARIADB_PASSWORD); }
void Settings::setDbMariadbPassword(const QString &v){ SecureSettings::instance().set(SecureKey::DB_MARIADB_PASSWORD, v); }

// ---------------------------------------------------------------------------
// Radio — Hamlib
// ---------------------------------------------------------------------------

bool Settings::hamlibEnabled() const        { return get("radio/hamlib/enabled", false); }
void Settings::setHamlibEnabled(bool v)     { put("radio/hamlib/enabled", v); }

int Settings::hamlibRigModel() const        { return get("radio/hamlib/rigmodel", 1); }
void Settings::setHamlibRigModel(int v)     { put("radio/hamlib/rigmodel", v); }

QString Settings::hamlibConnectionType() const { return get("radio/hamlib/conntype", QStringLiteral("serial")); }
void Settings::setHamlibConnectionType(const QString &v){ put("radio/hamlib/conntype", v); }

QString Settings::hamlibSerialDevice() const { return get("radio/hamlib/device", QStringLiteral("/dev/ttyUSB0")); }
void Settings::setHamlibSerialDevice(const QString &v){ put("radio/hamlib/device", v); }

int Settings::hamlibBaudRate() const        { return get("radio/hamlib/baud", 9600); }
void Settings::setHamlibBaudRate(int v)     { put("radio/hamlib/baud", v); }

int Settings::hamlibDataBits() const        { return get("radio/hamlib/databits", 8); }
void Settings::setHamlibDataBits(int v)     { put("radio/hamlib/databits", v); }

int Settings::hamlibStopBits() const        { return get("radio/hamlib/stopbits", 1); }
void Settings::setHamlibStopBits(int v)     { put("radio/hamlib/stopbits", v); }

QString Settings::hamlibParity() const      { return get("radio/hamlib/parity", QStringLiteral("None")); }
void Settings::setHamlibParity(const QString &v)    { put("radio/hamlib/parity", v); }

QString Settings::hamlibFlowControl() const { return get("radio/hamlib/flow", QStringLiteral("None")); }
void Settings::setHamlibFlowControl(const QString &v){ put("radio/hamlib/flow", v); }

QString Settings::hamlibNetworkHost() const { return get("radio/hamlib/nethost", QStringLiteral("localhost")); }
void Settings::setHamlibNetworkHost(const QString &v){ put("radio/hamlib/nethost", v); }

int Settings::hamlibNetworkPort() const     { return get("radio/hamlib/netport", 4532); }
void Settings::setHamlibNetworkPort(int v)  { put("radio/hamlib/netport", v); }

// ---------------------------------------------------------------------------
// Radio — TCI
// ---------------------------------------------------------------------------

bool Settings::tciEnabled() const           { return get("radio/tci/enabled", false); }
void Settings::setTciEnabled(bool v)        { put("radio/tci/enabled", v); }

QString Settings::tciHost() const           { return get("radio/tci/host", QStringLiteral("localhost")); }
void Settings::setTciHost(const QString &v) { put("radio/tci/host", v); }

int Settings::tciPort() const               { return get("radio/tci/port", 50001); }
void Settings::setTciPort(int v)            { put("radio/tci/port", v); }

// ---------------------------------------------------------------------------
// QSL — LoTW
// ---------------------------------------------------------------------------

bool Settings::lotwEnabled() const          { return get("qsl/lotw/enabled", false); }
void Settings::setLotwEnabled(bool v)       { put("qsl/lotw/enabled", v); }

QString Settings::lotwTqslPath() const      { return get("qsl/lotw/tqslpath", QString()); }
void Settings::setLotwTqslPath(const QString &v)    { put("qsl/lotw/tqslpath", v); }

QString Settings::lotwCallsign() const      { return get("qsl/lotw/callsign", QString()); }
void Settings::setLotwCallsign(const QString &v)    { put("qsl/lotw/callsign", v); }

QString Settings::lotwStationLocation() const           { return get("qsl/lotw/station_location", QString()); }
void Settings::setLotwStationLocation(const QString &v) { put("qsl/lotw/station_location", v); }

QString Settings::lotwPassword() const      { return SecureSettings::instance().get(SecureKey::LOTW_PASSWORD); }
void Settings::setLotwPassword(const QString &v)    { SecureSettings::instance().set(SecureKey::LOTW_PASSWORD, v); }

// ---------------------------------------------------------------------------
// QSL — eQSL
// ---------------------------------------------------------------------------

bool Settings::eqslEnabled() const          { return get("qsl/eqsl/enabled", false); }
void Settings::setEqslEnabled(bool v)       { put("qsl/eqsl/enabled", v); }

QString Settings::eqslUsername() const      { return get("qsl/eqsl/username", QString()); }
void Settings::setEqslUsername(const QString &v)    { put("qsl/eqsl/username", v); }

QString Settings::eqslNickname() const      { return get("qsl/eqsl/nickname", QString()); }
void Settings::setEqslNickname(const QString &v)    { put("qsl/eqsl/nickname", v); }

QString Settings::eqslPassword() const      { return SecureSettings::instance().get(SecureKey::EQSL_PASSWORD); }
void Settings::setEqslPassword(const QString &v)    { SecureSettings::instance().set(SecureKey::EQSL_PASSWORD, v); }

// ---------------------------------------------------------------------------
// QSL — QRZ
// ---------------------------------------------------------------------------

bool Settings::qrzEnabled() const           { return get("qsl/qrz/enabled", false); }
void Settings::setQrzEnabled(bool v)        { put("qsl/qrz/enabled", v); }

QString Settings::qrzUsername() const       { return get("qsl/qrz/username", QString()); }
void Settings::setQrzUsername(const QString &v)     { put("qsl/qrz/username", v); }

QString Settings::qrzApiKey() const         { return SecureSettings::instance().get(SecureKey::QRZ_API_KEY); }
void Settings::setQrzApiKey(const QString &v)       { SecureSettings::instance().set(SecureKey::QRZ_API_KEY, v); }

// ---------------------------------------------------------------------------
// QSL — ClubLog
// ---------------------------------------------------------------------------

bool Settings::clublogEnabled() const       { return get("qsl/clublog/enabled", false); }
void Settings::setClublogEnabled(bool v)    { put("qsl/clublog/enabled", v); }

QString Settings::clublogEmail() const      { return get("qsl/clublog/email", QString()); }
void Settings::setClublogEmail(const QString &v)    { put("qsl/clublog/email", v); }

QString Settings::clublogCallsign() const   { return get("qsl/clublog/callsign", QString()); }
void Settings::setClublogCallsign(const QString &v) { put("qsl/clublog/callsign", v); }

QString Settings::clublogPassword() const   { return SecureSettings::instance().get(SecureKey::CLUBLOG_PASSWORD); }
void Settings::setClublogPassword(const QString &v) { SecureSettings::instance().set(SecureKey::CLUBLOG_PASSWORD, v); }

QString Settings::clublogAppKey() const     { return SecureSettings::instance().get(SecureKey::CLUBLOG_APP_KEY); }
void Settings::setClublogAppKey(const QString &v)   { SecureSettings::instance().set(SecureKey::CLUBLOG_APP_KEY, v); }

// ---------------------------------------------------------------------------
// QSL — last download timestamps
// ---------------------------------------------------------------------------

QDate Settings::qslLastDownloadDate(const QString &service) const
{
    const QString key = QStringLiteral("qsl/last_download/") + service.toLower().remove(QLatin1Char(' '));
    const QVariant v = get(key, QVariant());
    const QDate stored = v.toDate();
    return stored.isValid() ? stored : QDate::currentDate().addDays(-90);
}

void Settings::setQslLastDownloadDate(const QString &service, const QDate &date)
{
    const QString key = QStringLiteral("qsl/last_download/") + service.toLower().remove(QLatin1Char(' '));
    put(key, date);
}

// ---------------------------------------------------------------------------
// WSJT-X
// ---------------------------------------------------------------------------

bool    Settings::wsjtxEnabled() const              { return get("wsjtx/enabled", false); }
void    Settings::setWsjtxEnabled(bool v)           { put("wsjtx/enabled", v); }

quint16 Settings::wsjtxPort() const                 { return static_cast<quint16>(get("wsjtx/port", 2237)); }
void    Settings::setWsjtxPort(quint16 v)           { put("wsjtx/port", static_cast<int>(v)); }

QString Settings::wsjtxUdpAddress() const            { return get("wsjtx/udp_address", QStringLiteral("224.0.0.1")); }
void    Settings::setWsjtxUdpAddress(const QString &v) { put("wsjtx/udp_address", v); }

bool    Settings::wsjtxAutoLog() const              { return get("wsjtx/auto_log", true); }
void    Settings::setWsjtxAutoLog(bool v)           { put("wsjtx/auto_log", v); }

// ---------------------------------------------------------------------------
// UI state
// ---------------------------------------------------------------------------

QByteArray Settings::mainWindowGeometry() const        { return get("ui/geometry", QByteArray()); }
void Settings::setMainWindowGeometry(const QByteArray &v){ put("ui/geometry", v); }

QByteArray Settings::mainWindowState() const           { return get("ui/windowstate", QByteArray()); }
void Settings::setMainWindowState(const QByteArray &v) { put("ui/windowstate", v); }

QByteArray Settings::splitterState() const             { return get("ui/splitter", QByteArray()); }
void Settings::setSplitterState(const QByteArray &v)   { put("ui/splitter", v); }
