// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ryan Butler (NF0T)
//
// Characterization tests: pin CtyDatLookupProvider behaviour before it moves
// into nf0t-core. Configuration is injected through CtyDatConfig.

#include <memory>

#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

#include "lookup/CtyDatLookupProvider.h"

class TestCtyDat : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void loadsCustomFile();
    void needsMoreThan100Entities();
    void disabledIsUnavailable();
    void enabledAndPathAreReadThroughConfig();
    void lookup_data();
    void lookup();
    void lookupFailures();

private:
    QString writeFixture(const QString &name, int fillerCount);
    std::unique_ptr<CtyDatLookupProvider> makeProvider(const QString &path);

    QTemporaryDir m_dir;
    QString m_fixture;
    bool m_enabled = true;
};

namespace {
// Modelled on real CTY.dat records. Longitudes are West-positive in the file.
const char *kEntities =
    "United States:            05:  08:  NA:   37.53:    91.67:     5.0:  K:\n"
    "    AA,K,N,W,W6(3)[6],\n"
    "    =W1AW(4)[7]{SA}<10.00/20.00>;\n"
    "Canada:                   05:  09:  NA:   44.35:    78.75:     5.0:  VE:\n"
    "    VA,VE,VE3(4)[4];\n"
    "Hawaii:                   31:  61:  OC:   21.12:   157.48:    10.0:  KH6:\n"
    "    AH6,KH6,NH6,WH6;\n"
    "Fed. Rep. of Germany:     14:  28:  EU:   51.00:   -10.00:    -1.0:  DL:\n"
    "    DA,DL;\n";
}

void TestCtyDat::initTestCase()
{
    QVERIFY(m_dir.isValid());
    qRegisterMetaType<CallsignLookupResult>();
    // Keeps the provider's AppLocalDataLocation fallback away from a real install.
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setOrganizationName(QStringLiteral("NF0T-Tests"));
    QCoreApplication::setApplicationName(QStringLiteral("test_ctydat"));

    m_fixture = writeFixture(QStringLiteral("cty.dat"), 97);   // 4 + 97 = 101 entities
}

QString TestCtyDat::writeFixture(const QString &name, int fillerCount)
{
    QString text = QString::fromUtf8(kEntities);
    for (int i = 0; i < fillerCount; ++i)
        text += QStringLiteral("Filler %1:  1:  1:  AN:  0.00:  0.00:  0.0:  *F%1:\n    =QQ%1ZZ;\n").arg(i);

    const QString path = m_dir.filePath(name);
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) return {};
    f.write(text.toLatin1());
    return path;
}

std::unique_ptr<CtyDatLookupProvider> TestCtyDat::makeProvider(const QString &path)
{
    m_enabled = true;
    return std::make_unique<CtyDatLookupProvider>(CtyDatConfig{
        .enabled     = [this] { return m_enabled; },
        .customPath  = [path] { return path; },
        .bundledPath = {},
    });
}

void TestCtyDat::loadsCustomFile()
{
    const auto p = makeProvider(m_fixture);
    QVERIFY(p->isAvailable());
    QCOMPARE(p->entityCount(), 101);
    QCOMPARE(p->fileDate(), QFileInfo(m_fixture).lastModified().toString(QStringLiteral("yyyy-MM-dd")));
    QCOMPARE(p->displayName(), QStringLiteral("CTY.dat"));
}

void TestCtyDat::needsMoreThan100Entities()
{
    // Current behaviour: a file with 100 or fewer entities is treated as
    // corrupt and not used.
    const auto p = makeProvider(writeFixture(QStringLiteral("small.dat"), 96));
    QCOMPARE(p->entityCount(), 100);
    QVERIFY(!p->isAvailable());

    QSignalSpy failed(p.get(), &CallsignLookupProvider::lookupFailed);
    p->lookup(QStringLiteral("W1ABC"));
    QCOMPARE(failed.count(), 1);
}

void TestCtyDat::disabledIsUnavailable()
{
    // enabled() is read on every call, not captured at construction.
    const auto p = makeProvider(m_fixture);
    m_enabled = false;
    QVERIFY(!p->isAvailable());
    m_enabled = true;
    QVERIFY(p->isAvailable());
}

void TestCtyDat::enabledAndPathAreReadThroughConfig()
{
    // Unset callables mean "disabled" and "no custom path"; with no bundled
    // file either, nothing loads.
    CtyDatLookupProvider bare{CtyDatConfig{}};
    QVERIFY(!bare.isAvailable());
    QCOMPARE(bare.entityCount(), 0);

    // A missing custom path falls through to the bundled path, reported as such.
    CtyDatLookupProvider fallback{CtyDatConfig{
        .enabled     = [] { return true; },
        .customPath  = [this] { return m_dir.filePath(QStringLiteral("missing.dat")); },
        .bundledPath = m_fixture,
    }};
    QVERIFY(fallback.isAvailable());
    QCOMPARE(fallback.entityCount(), 101);
    QCOMPARE(fallback.fileDate(), QStringLiteral("bundled"));
}

void TestCtyDat::lookup_data()
{
    QTest::addColumn<QString>("call");
    QTest::addColumn<QString>("country");
    QTest::addColumn<int>("cq");
    QTest::addColumn<int>("itu");
    QTest::addColumn<QString>("cont");
    QTest::addColumn<double>("lat");
    QTest::addColumn<double>("lon");

    QTest::newRow("prefix W")            << "W1ABC"     << "United States"        << 5  << 8  << "NA" << 37.53 << -91.67;
    QTest::newRow("lower case")          << "w1abc"     << "United States"        << 5  << 8  << "NA" << 37.53 << -91.67;
    QTest::newRow("longest prefix W6")   << "W6XYZ"     << "United States"        << 3  << 6  << "NA" << 37.53 << -91.67;
    QTest::newRow("longest prefix KH6")  << "KH6ABC"    << "Hawaii"               << 31 << 61 << "OC" << 21.12 << -157.48;
    QTest::newRow("exact with overrides")<< "W1AW"      << "United States"        << 4  << 7  << "SA" << 10.0  << -20.0;
    QTest::newRow("exact via base call") << "W1AW/P"    << "United States"        << 4  << 7  << "SA" << 10.0  << -20.0;
    QTest::newRow("outer prefix")        << "VE3/W1ABC" << "Canada"               << 4  << 4  << "NA" << 44.35 << -78.75;
    // Current behaviour: an exact match on the base call wins over an outer
    // prefix, so W1AW operating from Canada resolves to the United States.
    QTest::newRow("exact beats prefix")  << "VE3/W1AW"  << "United States"        << 4  << 7  << "SA" << 10.0  << -20.0;
    // Current behaviour: a trailing location is not used for DXCC matching.
    QTest::newRow("trailing location")   << "W1ABC/VE3" << "United States"        << 5  << 8  << "NA" << 37.53 << -91.67;
    QTest::newRow("east longitude")      << "DL1ABC"    << "Fed. Rep. of Germany" << 14 << 28 << "EU" << 51.0  << 10.0;
}

void TestCtyDat::lookup()
{
    QFETCH(QString, call);
    QFETCH(QString, country);
    QFETCH(int, cq);
    QFETCH(int, itu);
    QFETCH(QString, cont);
    QFETCH(double, lat);
    QFETCH(double, lon);

    const auto p = makeProvider(m_fixture);
    QSignalSpy ready(p.get(), &CallsignLookupProvider::resultReady);
    QSignalSpy failed(p.get(), &CallsignLookupProvider::lookupFailed);
    p->lookup(call);

    // Synchronous: the signal has fired before lookup() returns.
    QCOMPARE(failed.count(), 0);
    QCOMPARE(ready.count(), 1);
    QCOMPARE(ready.at(0).at(0).toString(), call);
    const auto r = ready.at(0).at(1).value<CallsignLookupResult>();
    QCOMPARE(r.callsign, call);   // the raw input, not normalised
    QCOMPARE(r.country, country);
    QCOMPARE(r.cqZone, cq);
    QCOMPARE(r.ituZone, itu);
    QCOMPARE(r.cont, cont);
    QVERIFY(r.lat.has_value() && r.lon.has_value());
    QCOMPARE(*r.lat, lat);
    QCOMPARE(*r.lon, lon);
    QCOMPARE(r.dxcc, 0);          // CTY.dat carries no DXCC entity numbers
    QVERIFY(r.name.isEmpty());
    QVERIFY(r.gridsquare.isEmpty());
}

void TestCtyDat::lookupFailures()
{
    const auto p = makeProvider(m_fixture);
    QSignalSpy ready(p.get(), &CallsignLookupProvider::resultReady);
    QSignalSpy failed(p.get(), &CallsignLookupProvider::lookupFailed);

    p->lookup(QStringLiteral("123"));      // invalid callsign
    p->lookup(QStringLiteral("ZZ9ZZ"));    // valid, no matching prefix
    QCOMPARE(ready.count(), 0);
    QCOMPARE(failed.count(), 2);
    QCOMPARE(failed.at(0).at(0).toString(), QStringLiteral("123"));
    QCOMPARE(failed.at(1).at(0).toString(), QStringLiteral("ZZ9ZZ"));
}

QTEST_GUILESS_MAIN(TestCtyDat)
#include "test_ctydat.moc"
