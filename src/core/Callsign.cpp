// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ryan Butler (NF0T)
#include "Callsign.h"

#include <QRegularExpression>

// ---------------------------------------------------------------------------
// Validation regex
// [prefix/] base [/suffix]
// base = 1–3 alphanum  +  digit  +  1–4 letters
// ---------------------------------------------------------------------------
static const QRegularExpression BASE_RE(
    QStringLiteral(R"([A-Z0-9]{1,3}[0-9][A-Z]{1,4})"));

static const QRegularExpression FULL_RE(
    QStringLiteral(
        R"(^(?:[A-Z0-9]{1,3}/)?[A-Z0-9]{1,3}[0-9][A-Z]{1,4}(?:/[A-Z0-9]{1,4})?$)"));

// ---------------------------------------------------------------------------
// Callsign implementation
// ---------------------------------------------------------------------------

Callsign::Callsign(const QString &raw)
{
    parse(raw.toUpper().trimmed());
}

bool Callsign::isValid(const QString &raw)
{
    return FULL_RE.match(raw.toUpper().trimmed()).hasMatch();
}

bool Callsign::looksLikeBase(const QString &s)
{
    return BASE_RE.match(s).captured() == s;
}

void Callsign::parse(const QString &raw)
{
    if (!FULL_RE.match(raw).hasMatch())
        return;

    m_full  = raw;
    m_valid = true;

    const QStringList parts = raw.split(QLatin1Char('/'));

    if (parts.size() == 3) {
        m_base   = parts[1];
        m_suffix = parts[2];
    } else if (parts.size() == 2) {
        if (looksLikeBase(parts[1])) {
            m_base = parts[1];
        } else {
            m_base   = parts[0];
            m_suffix = parts[1];
        }
    } else {
        m_base = parts[0];
    }
}

QString Callsign::operationalPrefix() const
{
    // If there is an outer prefix (e.g. "VE3" in "VE3/W1AW/P"), use it.
    // An outer prefix is the first slash-segment when it does NOT match the
    // full base-call pattern (i.e. it lacks the trailing letter group).
    const QStringList parts = m_full.split(QLatin1Char('/'));
    if (parts.size() >= 2 && !looksLikeBase(parts[0]))
        return parts[0];
    return m_base;
}
