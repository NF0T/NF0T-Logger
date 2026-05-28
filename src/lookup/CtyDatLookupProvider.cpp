// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ryan Butler (NF0T)
#include "CtyDatLookupProvider.h"

#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTextStream>

#include "app/settings/Settings.h"
#include "core/Callsign.h"

// ---------------------------------------------------------------------------
// Construction / file loading
// ---------------------------------------------------------------------------

CtyDatLookupProvider::CtyDatLookupProvider(QObject *parent)
    : CallsignLookupProvider(parent)
{
    bool isBundled = false;
    const QString path = resolveFilePath(isBundled);
    load(path, isBundled);
}

bool CtyDatLookupProvider::isAvailable() const
{
    return m_loaded && Settings::instance().ctyDatEnabled();
}

QString CtyDatLookupProvider::resolveFilePath(bool &outIsBundled)
{
    outIsBundled = false;

    const QString custom = Settings::instance().ctyDatCustomPath();
    if (!custom.isEmpty() && QFile::exists(custom))
        return custom;

    const QString dataDir =
        QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    const QString installed = dataDir + QStringLiteral("/cty.dat");
    if (QFile::exists(installed))
        return installed;

    outIsBundled = true;
    return QStringLiteral(":/data/cty.dat");
}

void CtyDatLookupProvider::load(const QString &path, bool isBundled)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return;

    m_fileDate = isBundled
        ? QStringLiteral("bundled")
        : QFileInfo(path).lastModified().toString(QStringLiteral("yyyy-MM-dd"));

    QTextStream stream(&f);
    stream.setEncoding(QStringConverter::Latin1);

    QString currentHeader;
    QString aliasAccum;

    while (!stream.atEnd()) {
        const QString line = stream.readLine();
        if (line.trimmed().isEmpty())
            continue;

        if (!line[0].isSpace()) {
            if (!currentHeader.isEmpty())
                parseRecord(currentHeader, aliasAccum);
            currentHeader = line.trimmed();
            aliasAccum.clear();
        } else {
            aliasAccum += line.trimmed();
        }
    }
    if (!currentHeader.isEmpty())
        parseRecord(currentHeader, aliasAccum);

    m_loaded = m_entities.size() > 100;
}

// ---------------------------------------------------------------------------
// Parsing
// ---------------------------------------------------------------------------

static const QRegularExpression s_reCq  (QStringLiteral(R"(\((\d+)\))"));
static const QRegularExpression s_reItu (QStringLiteral(R"(\[(\d+)\])"));
static const QRegularExpression s_reCont(QStringLiteral(R"(\{([A-Z]{2})\})"));
static const QRegularExpression s_reLL  (QStringLiteral(R"(<([^>]+)>)"));

void CtyDatLookupProvider::parseRecord(const QString &header, const QString &aliasList)
{
    // Header format: Name:CQ:ITU:Cont:lat:lon:UTC:PrimaryPrefix:
    const QStringList parts = header.split(QLatin1Char(':'));
    if (parts.size() < 8)
        return;

    Entity entity;
    entity.name    = parts[0].trimmed();
    entity.cqZone  = parts[1].trimmed().toInt();
    entity.ituZone = parts[2].trimmed().toInt();
    entity.cont    = parts[3].trimmed();
    entity.lat     =  parts[4].trimmed().toDouble();
    entity.lon     = -parts[5].trimmed().toDouble();  // CTY.dat West-positive → standard East-positive

    const int entityIdx = m_entities.size();
    m_entities.append(entity);

    QString aliases = aliasList;
    if (aliases.endsWith(QLatin1Char(';')))
        aliases.chop(1);

    const QStringList tokens = aliases.split(QLatin1Char(','), Qt::SkipEmptyParts);
    for (const QString &raw : tokens) {
        const QString token = raw.trimmed();
        if (token.isEmpty())
            continue;

        const bool exact = token.startsWith(QLatin1Char('='));
        const QString body = exact ? token.mid(1) : token;

        // Key = leading alphanumeric + '/' characters (everything before first modifier)
        QString key;
        for (const QChar c : body) {
            if (c.isLetterOrNumber() || c == QLatin1Char('/'))
                key += c;
            else
                break;
        }
        if (key.isEmpty())
            continue;

        MatchResult mr;
        mr.entityIdx = entityIdx;
        mr.cqZone    = entity.cqZone;
        mr.ituZone   = entity.ituZone;
        mr.cont      = entity.cont;
        mr.lat       = entity.lat;
        mr.lon       = entity.lon;

        auto mCq = s_reCq.match(body);
        if (mCq.hasMatch())
            mr.cqZone = mCq.captured(1).toInt();

        auto mItu = s_reItu.match(body);
        if (mItu.hasMatch())
            mr.ituZone = mItu.captured(1).toInt();

        auto mCont = s_reCont.match(body);
        if (mCont.hasMatch())
            mr.cont = mCont.captured(1);

        auto mLL = s_reLL.match(body);
        if (mLL.hasMatch()) {
            const QStringList ll = mLL.captured(1).split(QLatin1Char('/'));
            if (ll.size() == 2) {
                mr.lat =  ll[0].toDouble();
                mr.lon = -ll[1].toDouble();
            }
        }

        if (exact)
            m_exactMap.insert(key.toUpper(), mr);
        else
            m_prefixMap.insert(key.toUpper(), mr);
    }
}

// ---------------------------------------------------------------------------
// Lookup
// ---------------------------------------------------------------------------

void CtyDatLookupProvider::lookup(const QString &rawCallsign)
{
    if (!m_loaded) {
        emit lookupFailed(rawCallsign, tr("CTY.dat not loaded"));
        return;
    }

    const Callsign cs(rawCallsign);
    if (!cs.isValid()) {
        emit lookupFailed(rawCallsign, tr("Invalid callsign"));
        return;
    }

    // 1. Exact match — try full callsign, then base callsign
    for (const QString &key : {cs.full(), cs.base()}) {
        auto it = m_exactMap.constFind(key);
        if (it != m_exactMap.constEnd()) {
            emit resultReady(rawCallsign, buildResult(rawCallsign, *it));
            return;
        }
    }

    // 2. Prefix match — progressively shorten the operational prefix
    const QString opPrefix = cs.operationalPrefix().toUpper();
    for (int len = opPrefix.size(); len > 0; --len) {
        auto it = m_prefixMap.constFind(opPrefix.left(len));
        if (it != m_prefixMap.constEnd()) {
            emit resultReady(rawCallsign, buildResult(rawCallsign, *it));
            return;
        }
    }

    emit lookupFailed(rawCallsign,
        tr("No CTY.dat match for %1").arg(rawCallsign));
}

CallsignLookupResult CtyDatLookupProvider::buildResult(
    const QString &callsign, const MatchResult &m) const
{
    const Entity &e = m_entities.at(m.entityIdx);
    CallsignLookupResult r;
    r.callsign = callsign;
    r.country  = e.name;
    r.cont     = m.cont;
    r.cqZone   = m.cqZone;
    r.ituZone  = m.ituZone;
    r.lat      = m.lat;
    r.lon      = m.lon;
    return r;
}
