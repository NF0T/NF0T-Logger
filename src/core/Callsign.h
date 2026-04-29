// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ryan Butler (NF0T)
#pragma once

#include <QString>

/// Parses and validates an amateur radio callsign.
///
/// Handles the standard ITU format [prefix/]base[/suffix], where the base
/// portion contains 1–3 alphanumeric characters, one district digit, and
/// 1–4 letters (e.g. W1AW, VE3XYZ, 5B4AHJ).
///
/// Includes a built-in prefix→DXCC table for entity pre-fill.  Coverage is
/// representative but not exhaustive — QRZ lookup results always take
/// priority when available.
class Callsign
{
public:
    explicit Callsign(const QString &raw);

    /// Returns true if raw looks like a plausible amateur callsign.
    static bool isValid(const QString &raw);

    bool isValid() const { return m_valid; }

    /// Normalized full callsign (uppercase, trimmed).
    QString full() const { return m_full; }

    /// Base callsign without outer prefix or portable suffix.
    /// e.g. "W1AW" from "VE3/W1AW/P".  Use this for station lookups.
    QString base() const { return m_base; }

    /// Portable / operational suffix (e.g. "P", "M", "MM"); empty if none.
    QString suffix() const { return m_suffix; }

    // DXCC entity data derived from the built-in prefix table.
    // Empty / zero when the prefix is not in the table.
    QString country()   const { return m_country; }
    QString continent() const { return m_continent; }
    int     dxcc()      const { return m_dxcc; }
    int     cqZone()    const { return m_cqZone; }
    int     ituZone()   const { return m_ituZone; }

private:
    void parse(const QString &raw);
    void resolvePrefix(const QString &lookupPrefix);

    static bool    looksLikeBase(const QString &s);
    static QString extractCallPrefix(const QString &base);

    bool    m_valid     = false;
    QString m_full;
    QString m_base;
    QString m_suffix;
    QString m_country;
    QString m_continent;
    int     m_dxcc    = 0;
    int     m_cqZone  = 0;
    int     m_ituZone = 0;
};
