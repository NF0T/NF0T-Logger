// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ryan Butler (NF0T)
#pragma once

#include "CallsignLookupProvider.h"
#include "CallsignLookupResult.h"

#include <QHash>
#include <QString>
#include <QVector>

/// Offline DXCC lookup provider backed by the CTY.dat country file (AD1C / Big Cty).
///
/// Parses the file at construction time and holds the entity table in memory.
/// lookup() is synchronous — emits resultReady before returning.
///
/// Populates: country, cont, cqZone, ituZone, lat, lon.
/// Does not populate: dxcc, name, qth, state, gridsquare, licenseClass, or image fields.
///
/// File resolution order:
///   1. Settings::ctyDatCustomPath() if non-empty and the file exists
///   2. QStandardPaths::AppLocalDataLocation/cty.dat  (user-updated)
///   3. :/data/cty.dat                                 (bundled Qt resource)
class CtyDatLookupProvider : public CallsignLookupProvider
{
    Q_OBJECT

public:
    explicit CtyDatLookupProvider(QObject *parent = nullptr);

    QString displayName() const override { return QStringLiteral("CTY.dat"); }
    bool    isAvailable() const override;
    void    lookup(const QString &callsign) override;

    /// Modification date of the loaded file as "yyyy-MM-dd", or "bundled".
    QString fileDate()    const { return m_fileDate; }
    int     entityCount() const { return m_entities.size(); }

private:
    struct Entity {
        QString name;
        int     cqZone  = 0;
        int     ituZone = 0;
        QString cont;
        double  lat = 0.0;
        double  lon = 0.0;   // East-positive (standard); CTY.dat West-positive is negated on load
    };

    struct MatchResult {
        int     entityIdx = -1;
        int     cqZone    = 0;
        int     ituZone   = 0;
        QString cont;
        double  lat = 0.0;
        double  lon = 0.0;
    };

    static QString resolveFilePath(bool &outIsBundled);
    void load(const QString &path, bool isBundled);
    void parseRecord(const QString &header, const QString &aliasList);
    CallsignLookupResult buildResult(const QString &callsign,
                                     const MatchResult &m) const;

    QVector<Entity>             m_entities;
    QHash<QString, MatchResult> m_exactMap;   // key = exact callsign (uppercase, no leading '=')
    QHash<QString, MatchResult> m_prefixMap;  // key = prefix string  (uppercase)

    bool    m_loaded   = false;
    QString m_fileDate;
};
