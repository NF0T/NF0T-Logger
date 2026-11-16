// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ryan Butler (NF0T)
//
// Characterization tests: pin BandPlan's current behaviour before it moves
// into nf0t-core.

#include <QtTest>

#include "core/BandPlan.h"

class TestBandPlan : public QObject
{
    Q_OBJECT

private slots:
    void freqToBand_data();
    void freqToBand();
    void tableIsOrderedAndConsistent();
    void bandDefaults();
    void modeTable();
};

void TestBandPlan::freqToBand_data()
{
    QTest::addColumn<double>("mhz");
    QTest::addColumn<QString>("band");

    QTest::newRow("FT8 20m")          << 14.074    << "20m";
    QTest::newRow("20m low edge")     << 14.0      << "20m";
    QTest::newRow("20m high edge")    << 14.35     << "20m";
    QTest::newRow("just above 20m")   << 14.3501   << "";
    QTest::newRow("just below 20m")   << 13.999    << "";
    QTest::newRow("160m low edge")    << 1.8       << "160m";
    QTest::newRow("60m low edge")     << 5.0605    << "60m";
    QTest::newRow("40m high edge")    << 7.3       << "40m";
    QTest::newRow("2m")               << 146.52    << "2m";
    QTest::newRow("70cm")             << 432.1     << "70cm";
    // Current behaviour: 54.000 is both the top of 6m and the bottom of 5m;
    // the first table entry wins.
    QTest::newRow("6m/5m shared edge")<< 54.0      << "6m";
    QTest::newRow("just into 5m")     << 54.0001   << "5m";
    QTest::newRow("submm top")        << 7500000.0 << "submm";
    QTest::newRow("above everything") << 7500001.0 << "";
    QTest::newRow("zero")             << 0.0       << "";
    QTest::newRow("negative")         << -14.074   << "";
}

void TestBandPlan::freqToBand()
{
    QFETCH(double, mhz);
    QFETCH(QString, band);
    QCOMPARE(BandPlan::freqToBand(mhz), band);
}

void TestBandPlan::tableIsOrderedAndConsistent()
{
    const BandPlan::BandEntry *e = BandPlan::allEntries();
    const int n = BandPlan::entryCount();
    QCOMPARE(n, 33);

    const QStringList names = BandPlan::bandNames();
    QCOMPARE(names.size(), n);
    QCOMPARE(names.first(), QStringLiteral("2190m"));
    QCOMPARE(names.last(), QStringLiteral("submm"));

    for (int i = 0; i < n; ++i) {
        QCOMPARE(names.at(i), QString::fromLatin1(e[i].name));
        QVERIFY2(e[i].freqLow <= e[i].freqHigh, e[i].name);
        QVERIFY2(e[i].defaultFreq >= e[i].freqLow && e[i].defaultFreq <= e[i].freqHigh, e[i].name);
        if (qstrcmp(e[i].name, "5m") != 0)
            QCOMPARE(BandPlan::freqToBand(e[i].defaultFreq), QString::fromLatin1(e[i].name));
        if (i > 0)
            QVERIFY2(e[i].freqLow >= e[i - 1].freqHigh, e[i].name);   // ascending, edges may touch
    }

    // Current behaviour: 5m's default frequency is its low edge, which it
    // shares with 6m, so it maps back to 6m.
    QCOMPARE(BandPlan::freqToBand(BandPlan::bandDefaultFreq(QStringLiteral("5m"))),
             QStringLiteral("6m"));
}

void TestBandPlan::bandDefaults()
{
    QCOMPARE(BandPlan::bandDefaultFreq(QStringLiteral("20m")), 14.225);
    QCOMPARE(BandPlan::bandDefaultFreq(QStringLiteral("2m")), 144.2);
    QCOMPARE(BandPlan::bandDefaultFreq(QStringLiteral("nope")), 0.0);
    // Current behaviour: band names are matched case-sensitively.
    QCOMPARE(BandPlan::bandDefaultFreq(QStringLiteral("20M")), 0.0);
}

void TestBandPlan::modeTable()
{
    const QStringList modes = BandPlan::modeNames();
    QCOMPARE(modes.size(), BandPlan::modeEntryCount());
    QCOMPARE(modes.first(), QStringLiteral("SSB"));
    QVERIFY(modes.contains(QStringLiteral("FT8")));
    QVERIFY(modes.contains(QStringLiteral("MFSK")));

    QCOMPARE(BandPlan::modeDefaultRst(QStringLiteral("SSB")), QStringLiteral("59"));
    QCOMPARE(BandPlan::modeDefaultRst(QStringLiteral("CW")), QStringLiteral("599"));
    QCOMPARE(BandPlan::modeDefaultRst(QStringLiteral("FT8")), QStringLiteral("+0"));
    QCOMPARE(BandPlan::modeDefaultRst(QStringLiteral("WSPR")), QStringLiteral("-10"));
    QCOMPARE(BandPlan::modeDefaultRst(QStringLiteral("RTTY")), QStringLiteral("599"));
    QCOMPARE(BandPlan::modeDefaultRst(QStringLiteral("UNKNOWN")), QStringLiteral("59"));
    // Current behaviour: mode names are matched case-sensitively.
    QCOMPARE(BandPlan::modeDefaultRst(QStringLiteral("cw")), QStringLiteral("59"));

    QCOMPARE(BandPlan::submodesForMode(QStringLiteral("SSB")),
             (QStringList{QStringLiteral("USB"), QStringLiteral("LSB")}));
    QVERIFY(BandPlan::submodesForMode(QStringLiteral("MFSK")).contains(QStringLiteral("FT4")));
    QVERIFY(BandPlan::submodesForMode(QStringLiteral("PSK")).contains(QStringLiteral("BPSK31")));
    QVERIFY(BandPlan::submodesForMode(QStringLiteral("CW")).isEmpty());
    QVERIFY(BandPlan::submodesForMode(QStringLiteral("UNKNOWN")).isEmpty());
}

QTEST_GUILESS_MAIN(TestBandPlan)
#include "test_bandplan.moc"
