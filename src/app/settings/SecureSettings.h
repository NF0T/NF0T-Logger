#pragma once

#include <QMap>
#include <QObject>
#include <QString>

/// Keychain-backed secure credential storage.
///
/// Wraps qtkeychain with an in-memory cache so callers always read
/// synchronously after the initial async load. All writes are async
/// (fire-and-forget); errors are emitted via error().
///
/// Usage:
///   1. Call SecureSettings::instance().loadAll() early in startup.
///   2. Connect to loaded() to know when it's safe to open the settings dialog.
///   3. Use get()/set()/remove() for individual credentials.
class SecureSettings : public QObject
{
    Q_OBJECT

public:
    static SecureSettings &instance();

    /// Fire async ReadPasswordJob for every known key.
    /// Emits loaded() when all jobs complete (succeess or failure).
    void loadAll();

    bool isLoaded() const { return m_loaded; }

    /// Returns value from cache (empty string if not found or not yet loaded).
    QString get(const QString &key) const;

    /// Updates cache immediately; writes to keychain async.
    void set(const QString &key, const QString &value);

    /// Removes from cache immediately; deletes from keychain async.
    void remove(const QString &key);

signals:
    void loaded();
    void error(const QString &key, const QString &message);

private:
    SecureSettings();

    static const char *SERVICE_NAME;
    static const QStringList &knownKeys();

    QMap<QString, QString> m_cache;
    int  m_pendingJobs = 0;
    bool m_loaded      = false;
};

/// Centralised key constants — use these everywhere instead of raw strings.
namespace SecureKey {
    inline constexpr const char *DB_MARIADB_PASSWORD  = "db/mariadb/password";
    inline constexpr const char *EQSL_PASSWORD        = "eqsl/password";
    inline constexpr const char *QRZ_API_KEY          = "qrz/apikey";
    inline constexpr const char *CLUBLOG_PASSWORD     = "clublog/password";
    inline constexpr const char *CLUBLOG_APP_KEY      = "clublog/appkey";
    inline constexpr const char *LOTW_PASSWORD        = "lotw/password";
}
