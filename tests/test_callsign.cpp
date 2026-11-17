// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ryan Butler (NF0T)
//
// Characterization tests: pin Callsign's current behaviour before it moves
// into nf0t-core. Rows marked "current behaviour" record what the code does
// today, not necessarily what it should do.

#include <QtTest>

#include "core/Callsign.h"

class TestCallsign : public QObject
{
    Q_OBJECT

private slots:
    void parse_data();
    void parse();
    void invalid_data();
    void invalid();
};

void TestCallsign::parse_data()
{
    QTest::addColumn<QString>("raw");
    QTest::addColumn<QString>("full");
    QTest::addColumn<QString>("base");
    QTest::addColumn<QString>("suffix");
    QTest::addColumn<QString>("opPrefix");

    QTest::newRow("plain")              << "W1AW"        << "W1AW"        << "W1AW"   << ""    << "W1AW";
    QTest::newRow("lower, padded")      << "  w1aw "     << "W1AW"        << "W1AW"   << ""    << "W1AW";
    QTest::newRow("digit-led prefix")   << "5B4AHJ"      << "5B4AHJ"      << "5B4AHJ" << ""    << "5B4AHJ";
    QTest::newRow("2E0")                << "2E0ABC"      << "2E0ABC"      << "2E0ABC" << ""    << "2E0ABC";
    QTest::newRow("3-char prefix")      << "3DA0RU"      << "3DA0RU"      << "3DA0RU" << ""    << "3DA0RU";
    QTest::newRow("4U1ITU")             << "4U1ITU"      << "4U1ITU"      << "4U1ITU" << ""    << "4U1ITU";
    QTest::newRow("1x1")                << "K1A"         << "K1A"         << "K1A"    << ""    << "K1A";
    QTest::newRow("portable suffix")    << "W1AW/P"      << "W1AW/P"      << "W1AW"   << "P"   << "W1AW";
    QTest::newRow("maritime mobile")    << "W1AW/MM"     << "W1AW/MM"     << "W1AW"   << "MM"  << "W1AW";
    QTest::newRow("4-char suffix")      << "K1ABC/QRPP"  << "K1ABC/QRPP"  << "K1ABC"  << "QRPP"<< "K1ABC";
    QTest::newRow("outer prefix")       << "VE3/W1AW"    << "VE3/W1AW"    << "W1AW"   << ""    << "VE3";
    QTest::newRow("outer prefix KH6")   << "KH6/W1AW"    << "KH6/W1AW"    << "W1AW"   << ""    << "KH6";
    QTest::newRow("prefix + suffix")    << "VE3/W1AW/P"  << "VE3/W1AW/P"  << "W1AW"   << "P"   << "VE3";
    // Current behaviour: a trailing location is a suffix, so the prefix used
    // for DXCC matching is the home call, not VE3.
    QTest::newRow("trailing location")  << "W1AW/VE3"    << "W1AW/VE3"    << "W1AW"   << "VE3" << "W1AW";
    // Current behaviour: a prefix that itself matches the base-call pattern
    // (VP2E, Anguilla) is not recognised as a prefix, so DXCC matching uses
    // the home call.
    QTest::newRow("call-shaped prefix") << "VP2E/W1AW"   << "VP2E/W1AW"   << "W1AW"   << ""    << "W1AW";
}

void TestCallsign::parse()
{
    QFETCH(QString, raw);
    QFETCH(QString, full);
    QFETCH(QString, base);
    QFETCH(QString, suffix);
    QFETCH(QString, opPrefix);

    QVERIFY(Callsign::isValid(raw));
    const Callsign cs(raw);
    QVERIFY(cs.isValid());
    QCOMPARE(cs.full(), full);
    QCOMPARE(cs.base(), base);
    QCOMPARE(cs.suffix(), suffix);
    QCOMPARE(cs.operationalPrefix(), opPrefix);
}

void TestCallsign::invalid_data()
{
    QTest::addColumn<QString>("raw");

    QTest::newRow("empty")              << "";
    QTest::newRow("no letters")         << "W1";
    QTest::newRow("digits only")        << "1234";
    QTest::newRow("5-letter suffix")    << "W1ABCDE";
    QTest::newRow("trailing slash")     << "W1AW/";
    QTest::newRow("leading slash")      << "/W1AW";
    QTest::newRow("hyphen")             << "W1AW-1";
    QTest::newRow("5-char suffix")      << "K1ABC/QRPPP";
    QTest::newRow("two suffixes")       << "W1AW/P/QRP/X";
}

void TestCallsign::invalid()
{
    QFETCH(QString, raw);

    QVERIFY(!Callsign::isValid(raw));
    const Callsign cs(raw);
    QVERIFY(!cs.isValid());
    QVERIFY(cs.full().isEmpty());
    QVERIFY(cs.base().isEmpty());
    QVERIFY(cs.suffix().isEmpty());
}

QTEST_GUILESS_MAIN(TestCallsign)
#include "test_callsign.moc"
