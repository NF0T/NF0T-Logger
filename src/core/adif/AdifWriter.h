// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ryan Butler (NF0T)
#pragma once

#include <QList>
#include <QString>

#include "core/logbook/Qso.h"

struct AdifWriterOptions {
    bool    includeHeader    = true;
    QString programId        = QStringLiteral("NF0T Logger");
    QString programVersion   = QStringLiteral(APP_VERSION);
    bool    includeAppFields = true;   // write APP_NF0T_* extensions (QRZ, ClubLog)
};

/// Writes Qso records in ADIF v3 ADI format.
class AdifWriter
{
public:
    using Options = AdifWriterOptions;

    static bool    writeFile  (const QString &path, const QList<Qso> &qsos,
                               const Options &opts = Options());
    static QString writeString(const QList<Qso> &qsos,
                               const Options &opts = Options());

private:
    static QString header (const Options &opts);
    static QString record (const Qso &qso, const Options &opts);

    // Field helpers — return empty string if value is empty/invalid (field omitted)
    static QString f(const QString &name, const QString &value);
    static QString f(const QString &name, double value, int decimals, bool skipIfZero = false);
    static QString f(const QString &name, int value, bool skipIfZero = true);
    static QString f(const QString &name, const QDate &date);
    static QString f(const QString &name, QChar status);   // QSL status — omit if 'N'
    static QString fLatLon(const QString &name, double degrees, bool isLat);
};
