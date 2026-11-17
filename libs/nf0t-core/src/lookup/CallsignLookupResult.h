// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ryan Butler (NF0T)
#pragma once

#include <optional>
#include <QMetaType>
#include <QString>

/// Data returned by a successful callsign lookup.
///
/// All fields are optional in the sense that a provider may leave any of them
/// empty / zero if the data is not available for a given callsign.
struct CallsignLookupResult
{
    QString callsign;       // canonical callsign as returned by the provider
    QString name;           // full name (first + last)
    QString firstName;
    QString lastName;
    QString qth;            // city / QTH description
    QString state;          // state / province
    QString county;
    QString country;
    QString gridsquare;     // Maidenhead locator
    QString cont;           // 2-char continent code (NA, EU, AS, AF, OC, SA, AN)
    int     dxcc    = 0;    // DXCC entity number (0 = unknown)
    int     cqZone  = 0;    // CQ zone (0 = unknown)
    int     ituZone = 0;    // ITU zone (0 = unknown)
    std::optional<double> lat;
    std::optional<double> lon;
    QString licenseClass;   // e.g. "Extra", "General", "Technician"
    QString email;
    QString url;            // biography URL
    QString imageUrl;       // profile photo URL (display only — not persisted)
};

Q_DECLARE_METATYPE(CallsignLookupResult)
