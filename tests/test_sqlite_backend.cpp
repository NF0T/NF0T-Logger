// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ryan Butler (NF0T)
//
// Characterization tests: pin SqliteBackend / SqlBackendBase behaviour before
// they move into nf0t-core. MariaDB shares SqlBackendBase but needs a server,
// so only its SQLite sibling is covered here.

#include <memory>

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QtTest>

#include "QsoFixtures.h"
#include "database/SqliteBackend.h"

class TestSqliteBackend : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void openRequiresPath();
    void initSchemaAppliesEveryMigration();
    void initSchemaIsIdempotent();
    void v2MigrationNormalisesModes();
    void insertFetchRoundTripIsLossless();
    void emptyValuesRoundTripAsDefaults();
    void duplicateIsRejectedNotThrown();
    void missingStationCallsign();
    void updateDeleteClear();
    void transactionRollback();
    void filters();
    void orderingAndPaging();

private:
    std::expected<qint64, QString> insert(Qso q) { return m_db->insertQso(q); }
    QList<int> schemaVersions();
    QString rawPath() const { return m_dir->filePath(QStringLiteral("log.db")); }

    std::unique_ptr<QTemporaryDir> m_dir;
    std::unique_ptr<SqliteBackend> m_db;
};

namespace {
// QVERIFY2 evaluates its message even when the check passes.
template<typename T>
QString errorOf(const std::expected<T, QString> &r) { return r.has_value() ? QString() : r.error(); }

Qso make(const QString &call, const QDateTime &on, const QString &band = QStringLiteral("20m"),
         const QString &mode = QStringLiteral("SSB"))
{
    Qso q;
    q.callsign   = call;
    q.datetimeOn = on;
    q.band       = band;
    q.freq       = 14.2;
    q.mode       = mode;
    q.stationCallsign = QStringLiteral("NF0T");
    return q;
}

QDateTime at(int day, int hour)
{
    return QDateTime(QDate(2026, 11, day), QTime(hour, 0), QTimeZone::utc());
}
} // namespace

void TestSqliteBackend::init()
{
    m_dir = std::make_unique<QTemporaryDir>();
    QVERIFY(m_dir->isValid());
    m_db = std::make_unique<SqliteBackend>();
    const auto opened = m_db->open({{QStringLiteral("path"), rawPath()}});
    QVERIFY2(opened.has_value(), qPrintable(errorOf(opened)));
    const auto schema = m_db->initSchema();
    QVERIFY2(schema.has_value(), qPrintable(errorOf(schema)));
}

void TestSqliteBackend::cleanup()
{
    m_db.reset();
    m_dir.reset();
}

QList<int> TestSqliteBackend::schemaVersions()
{
    QList<int> versions;
    {
        QSqlDatabase raw = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("raw"));
        raw.setDatabaseName(rawPath());
        if (raw.open()) {
            QSqlQuery q(raw);
            q.exec(QStringLiteral("SELECT version FROM schema_version ORDER BY version"));
            while (q.next()) versions << q.value(0).toInt();
        }
        raw.close();
    }
    QSqlDatabase::removeDatabase(QStringLiteral("raw"));
    return versions;
}

void TestSqliteBackend::openRequiresPath()
{
    SqliteBackend db;
    const auto r = db.open({});
    QVERIFY(!r.has_value());
    QVERIFY(!r.error().isEmpty());
    QVERIFY(!db.isOpen());
}

void TestSqliteBackend::initSchemaAppliesEveryMigration()
{
    QVERIFY(m_db->isOpen());
    QCOMPARE(schemaVersions(), (QList<int>{1, 2}));
    QCOMPARE(*m_db->qsoCount(), 0);
}

void TestSqliteBackend::initSchemaIsIdempotent()
{
    QVERIFY(m_db->initSchema().has_value());
    QVERIFY(m_db->initSchema().has_value());
    QCOMPARE(schemaVersions(), (QList<int>{1, 2}));
}

void TestSqliteBackend::v2MigrationNormalisesModes()
{
    // Rebuild the state of a v1 database holding pre-v2 mode names.
    {
        QSqlDatabase raw = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("v1"));
        raw.setDatabaseName(rawPath());
        QVERIFY(raw.open());
        QSqlQuery q(raw);
        QVERIFY(q.exec(QStringLiteral("DELETE FROM schema_version WHERE version = 2")));
        const char *rows[][3] = {
            {"A1", "PSK31",     nullptr},   // → PSK / PSK31
            {"A2", "FT4",       nullptr},   // → MFSK / FT4
            {"A3", "FT4",       "XYZ"},     // submode already set: untouched
            {"A4", "PACKET",    "KEEP"},    // rename keeps submode
            {"A5", "CONTESTIA", nullptr},   // → CONTESTI
            {"A6", "SSB",       "USB"},     // not in the map
        };
        for (const auto &r : rows) {
            QSqlQuery ins(raw);
            ins.prepare(QStringLiteral("INSERT INTO qsos (callsign, datetime_on, band, mode, submode) "
                                       "VALUES (:c, '2026-01-01T00:00:00Z', '20m', :m, :s)"));
            ins.bindValue(QStringLiteral(":c"), QString::fromLatin1(r[0]));
            ins.bindValue(QStringLiteral(":m"), QString::fromLatin1(r[1]));
            ins.bindValue(QStringLiteral(":s"), r[2] ? QVariant(QString::fromLatin1(r[2])) : QVariant());
            QVERIFY(ins.exec());
        }
        raw.close();
    }
    QSqlDatabase::removeDatabase(QStringLiteral("v1"));

    QVERIFY(m_db->initSchema().has_value());

    QHash<QString, QPair<QString, QString>> got;
    for (const Qso &q : *m_db->fetchQsos())
        got.insert(q.callsign, {q.mode, q.submode});
    QCOMPARE(got.value("A1"), qMakePair(QStringLiteral("PSK"),      QStringLiteral("PSK31")));
    QCOMPARE(got.value("A2"), qMakePair(QStringLiteral("MFSK"),     QStringLiteral("FT4")));
    QCOMPARE(got.value("A3"), qMakePair(QStringLiteral("FT4"),      QStringLiteral("XYZ")));
    QCOMPARE(got.value("A4"), qMakePair(QStringLiteral("PKT"),      QStringLiteral("KEEP")));
    QCOMPARE(got.value("A5"), qMakePair(QStringLiteral("CONTESTI"), QString()));
    QCOMPARE(got.value("A6"), qMakePair(QStringLiteral("SSB"),      QStringLiteral("USB")));

    // Current behaviour: the v1 step re-inserts its version row on re-run.
    QCOMPARE(schemaVersions(), (QList<int>{1, 1, 2}));
}

void TestSqliteBackend::insertFetchRoundTripIsLossless()
{
    Qso in = QsoFixtures::fullyPopulated();
    in.arrlCheck = QStringLiteral("75");   // the DB, unlike ADIF, keeps it separate

    const auto id = m_db->insertQso(in);
    QVERIFY2(id.has_value(), qPrintable(errorOf(id)));
    QVERIFY(*id > 0);
    QCOMPARE(in.id, *id);

    const auto rows = m_db->fetchQsos();
    QVERIFY(rows.has_value());
    QCOMPARE(rows->size(), 1);
    const Qso &out = rows->first();
    QCOMPARE(out.id, *id);
    COMPARE_QSO(out, in, 0.0);
    QCOMPARE(out.datetimeOn.offsetFromUtc(), 0);
}

void TestSqliteBackend::emptyValuesRoundTripAsDefaults()
{
    // Empty strings, zero ints and unset optionals are stored as NULL and
    // come back as the Qso defaults; QSL statuses come back as 'N'.
    const Qso in = make(QStringLiteral("W1AW"), at(1, 0));
    QVERIFY(insert(in).has_value());

    const Qso out = m_db->fetchQsos()->first();
    COMPARE_QSO(out, in, 0.0);

    QSqlDatabase raw = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("nulls"));
    raw.setDatabaseName(rawPath());
    QVERIFY(raw.open());
    {
        QSqlQuery q(raw);
        QVERIFY(q.exec(QStringLiteral("SELECT name, dxcc, lat, lotw_qsl_sent FROM qsos")));
        QVERIFY(q.next());
        QVERIFY(q.value(0).isNull());
        QVERIFY(q.value(1).isNull());
        QVERIFY(q.value(2).isNull());
        QCOMPARE(q.value(3).toString(), QStringLiteral("N"));
    }
    raw.close();
    raw = QSqlDatabase();
    QSqlDatabase::removeDatabase(QStringLiteral("nulls"));
}

void TestSqliteBackend::duplicateIsRejectedNotThrown()
{
    // The uniqueness key is (callsign, datetime_on, band, mode).
    QVERIFY(insert(make(QStringLiteral("W1AW"), at(1, 0))).has_value());

    const auto dup = insert(make(QStringLiteral("W1AW"), at(1, 0)));
    QVERIFY(!dup.has_value());
    QVERIFY(!dup.error().isEmpty());
    QCOMPARE(*m_db->qsoCount(), 1);

    QVERIFY(insert(make(QStringLiteral("W1AW"), at(1, 0), QStringLiteral("40m"))).has_value());
    QVERIFY(insert(make(QStringLiteral("W1AW"), at(1, 0), QStringLiteral("20m"), QStringLiteral("CW"))).has_value());
    QVERIFY(insert(make(QStringLiteral("W1AW"), at(1, 1))).has_value());
    QCOMPARE(*m_db->qsoCount(), 4);
}

void TestSqliteBackend::missingStationCallsign()
{
    // station_callsign is NOT NULL DEFAULT '', but it is bound directly rather
    // than through the empty-to-NULL helper, so an empty QString is stored as
    // '' while a null QString (a Qso nobody set it on) binds NULL and the
    // insert is rejected.
    Qso unset = make(QStringLiteral("W1AW"), at(1, 0));
    unset.stationCallsign = QString();
    const auto r = insert(unset);
    QVERIFY(!r.has_value());
    QVERIFY(r.error().contains(QStringLiteral("station_callsign"), Qt::CaseInsensitive));

    Qso empty = make(QStringLiteral("W1AW"), at(1, 0));
    empty.stationCallsign = QStringLiteral("");
    QVERIFY(insert(empty).has_value());
    QVERIFY(m_db->fetchQsos()->first().stationCallsign.isEmpty());
}

void TestSqliteBackend::updateDeleteClear()
{
    Qso a = make(QStringLiteral("W1AW"), at(1, 0));
    Qso b = make(QStringLiteral("K1ABC"), at(1, 1));
    QVERIFY(m_db->insertQso(a).has_value());
    QVERIFY(m_db->insertQso(b).has_value());

    a.name = QStringLiteral("Hiram");
    a.lotwQslSent = 'Y';
    QVERIFY(m_db->updateQso(a).has_value());
    QsoFilter byCall;
    byCall.exactCall = QStringLiteral("W1AW");
    const Qso updated = m_db->fetchQsos(byCall)->first();
    QCOMPARE(updated.id, a.id);
    QCOMPARE(updated.name, QStringLiteral("Hiram"));
    QCOMPARE(updated.lotwQslSent, QChar('Y'));

    QVERIFY(m_db->deleteQso(a.id).has_value());
    QCOMPARE(*m_db->qsoCount(), 1);
    QVERIFY(m_db->deleteQso(999999).has_value());   // missing id is not an error

    QVERIFY(m_db->clearQsos().has_value());
    QCOMPARE(*m_db->qsoCount(), 0);
    QCOMPARE(schemaVersions(), (QList<int>{1, 2}));   // schema survives New Log
}

void TestSqliteBackend::transactionRollback()
{
    QVERIFY(m_db->beginTransaction().has_value());
    QVERIFY(insert(make(QStringLiteral("W1AW"), at(1, 0))).has_value());
    QVERIFY(m_db->rollbackTransaction().has_value());
    QCOMPARE(*m_db->qsoCount(), 0);

    QVERIFY(m_db->beginTransaction().has_value());
    QVERIFY(insert(make(QStringLiteral("W1AW"), at(1, 0))).has_value());
    QVERIFY(m_db->commitTransaction().has_value());
    QCOMPARE(*m_db->qsoCount(), 1);
}

void TestSqliteBackend::filters()
{
    Qso a = make(QStringLiteral("W1AW"),  at(1, 0), QStringLiteral("20m"), QStringLiteral("SSB"));
    Qso b = make(QStringLiteral("W1AWX"), at(2, 0), QStringLiteral("40m"), QStringLiteral("CW"));
    Qso c = make(QStringLiteral("K1ABC"), at(3, 0), QStringLiteral("20m"), QStringLiteral("CW"));
    a.lotwQslSent = 'Y';  a.lotwQslRcvd = 'N';   // pending
    b.lotwQslSent = 'Y';  b.lotwQslRcvd = 'Y';   // confirmed
    for (Qso *q : {&a, &b, &c}) QVERIFY(m_db->insertQso(*q).has_value());

    auto calls = [this](const QsoFilter &f) {
        QStringList out;
        for (const Qso &q : *m_db->fetchQsos(f)) out << q.callsign;
        out.sort();
        return out;
    };

    QsoFilter f;
    f.call = QStringLiteral("1AW");
    QCOMPARE(calls(f), (QStringList{"W1AW", "W1AWX"}));
    QCOMPARE(*m_db->countQsos(f), 2);

    f = {};
    f.exactCall = QStringLiteral("W1AW");
    QCOMPARE(calls(f), (QStringList{"W1AW"}));

    f = {};
    f.band = QStringLiteral("20m");
    f.mode = QStringLiteral("CW");
    QCOMPARE(calls(f), (QStringList{"K1ABC"}));

    f = {};
    f.from = at(2, 0);
    f.to   = at(3, 0);
    QCOMPARE(calls(f), (QStringList{"K1ABC", "W1AWX"}));   // both bounds inclusive

    f = {};
    f.lotwPending = true;
    QCOMPARE(calls(f), (QStringList{"W1AW"}));

    f = {};
    f.lotwUnsent = true;
    QCOMPARE(calls(f), (QStringList{"K1ABC"}));
    QCOMPARE(*m_db->countQsos(f), 1);
}

void TestSqliteBackend::orderingAndPaging()
{
    for (int d = 1; d <= 5; ++d)
        QVERIFY(insert(make(QStringLiteral("W%1AA").arg(d), at(d, 0))).has_value());

    auto calls = [this](const QsoFilter &f) {
        QStringList out;
        for (const Qso &q : *m_db->fetchQsos(f)) out << q.callsign;
        return out;
    };

    QsoFilter f;   // default: newest first
    QCOMPARE(calls(f), (QStringList{"W5AA", "W4AA", "W3AA", "W2AA", "W1AA"}));

    f.ascending = true;
    f.limit = 2;
    f.offset = 1;
    QCOMPARE(calls(f), (QStringList{"W2AA", "W3AA"}));

    // An unknown sort column falls back to datetime_on rather than being
    // spliced into the SQL.
    f = {};
    f.orderBy = QStringLiteral("callsign; DROP TABLE qsos");
    QCOMPARE(calls(f).size(), 5);
    QCOMPARE(calls(f).first(), QStringLiteral("W5AA"));
}

QTEST_GUILESS_MAIN(TestSqliteBackend)
#include "test_sqlite_backend.moc"
