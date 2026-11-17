// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ryan Butler (NF0T)
//
// Characterization tests: pin Maidenhead's current behaviour before it moves
// into nf0t-core.

#include <cmath>
#include <numbers>

#include <QtTest>

#include "core/Maidenhead.h"

class TestMaidenhead : public QObject
{
    Q_OBJECT

private slots:
    void validity_data();
    void validity();
    void toLatLonCentres_data();
    void toLatLonCentres();
    void invalidGridLeavesOutputsUntouched();
    void fromLatLon_data();
    void fromLatLon();
    void roundTrip_data();
    void roundTrip();
    void haversineProperties();
    void gridDistances();
};

namespace {
constexpr double kTol = 1e-6;
constexpr double kEarthRadiusKm = 6371.0;
}

void TestMaidenhead::validity_data()
{
    QTest::addColumn<QString>("grid");
    QTest::addColumn<bool>("valid");

    QTest::newRow("field")            << "EM"       << true;
    QTest::newRow("square")           << "EM29"     << true;
    QTest::newRow("subsquare")        << "EM29jx"   << true;
    QTest::newRow("mixed case")       << "em29JX"   << true;
    QTest::newRow("corners AA00aa")   << "AA00aa"   << true;
    QTest::newRow("corners RR99xx")   << "RR99xx"   << true;
    QTest::newRow("empty")            << ""         << false;
    QTest::newRow("odd length")       << "EM2"      << false;
    QTest::newRow("field past R")     << "SA"       << false;
    QTest::newRow("letter in square") << "EM2a"     << false;
    QTest::newRow("subsquare past X") << "EM29jy"   << false;
    // Current behaviour: 8-character extended locators are not supported.
    QTest::newRow("8 characters")     << "EM29jx12" << false;
}

void TestMaidenhead::validity()
{
    QFETCH(QString, grid);
    QFETCH(bool, valid);
    QCOMPARE(Maidenhead::isValid(grid), valid);
}

void TestMaidenhead::toLatLonCentres_data()
{
    QTest::addColumn<QString>("grid");
    QTest::addColumn<double>("lat");
    QTest::addColumn<double>("lon");

    // Centre of the cell, not its south-west corner.
    QTest::newRow("field JJ")     << "JJ"     <<   5.0           <<  10.0;
    QTest::newRow("square EM29")  << "EM29"   <<  39.5           << -95.0;
    QTest::newRow("sub EM29jx")   << "EM29jx" <<  39.9791666667  << -95.2083333333;
    QTest::newRow("sub AA00aa")   << "AA00aa" << -89.9791666667  << -179.9583333333;
    QTest::newRow("sub RR99xx")   << "RR99xx" <<  89.9791666667  <<  179.9583333333;
}

void TestMaidenhead::toLatLonCentres()
{
    QFETCH(QString, grid);
    QFETCH(double, lat);
    QFETCH(double, lon);

    double gotLat = 0.0, gotLon = 0.0;
    QVERIFY(Maidenhead::toLatLon(grid, gotLat, gotLon));
    QVERIFY2(std::abs(gotLat - lat) < kTol, qPrintable(QString::number(gotLat, 'f', 10)));
    QVERIFY2(std::abs(gotLon - lon) < kTol, qPrintable(QString::number(gotLon, 'f', 10)));
}

void TestMaidenhead::invalidGridLeavesOutputsUntouched()
{
    double lat = 12.5, lon = -7.25;
    QVERIFY(!Maidenhead::toLatLon(QStringLiteral("ZZ99"), lat, lon));
    QCOMPARE(lat, 12.5);
    QCOMPARE(lon, -7.25);
}

void TestMaidenhead::fromLatLon_data()
{
    QTest::addColumn<double>("lat");
    QTest::addColumn<double>("lon");
    QTest::addColumn<int>("precision");
    QTest::addColumn<QString>("grid");

    QTest::newRow("6 char")            <<  39.979167 <<  -95.208333 << 6 << "EM29jx";
    QTest::newRow("4 char")            <<  39.979167 <<  -95.208333 << 4 << "EM29";
    QTest::newRow("bad precision -> 6")<<  39.979167 <<  -95.208333 << 5 << "EM29jx";
    QTest::newRow("origin")            <<   0.0      <<    0.0      << 6 << "JJ00aa";
    // The +90/+180 edges are clamped into the last cell rather than rejected.
    QTest::newRow("north-east edge")   <<  90.0      <<  180.0      << 6 << "RR99xx";
    QTest::newRow("south-west edge")   << -90.0      << -180.0      << 6 << "AA00aa";
    QTest::newRow("lat out of range")  <<  90.0001   <<    0.0      << 6 << "";
    QTest::newRow("lon out of range")  <<   0.0      << -180.0001   << 6 << "";
}

void TestMaidenhead::fromLatLon()
{
    QFETCH(double, lat);
    QFETCH(double, lon);
    QFETCH(int, precision);
    QFETCH(QString, grid);
    QCOMPARE(Maidenhead::fromLatLon(lat, lon, precision), grid);
}

void TestMaidenhead::roundTrip_data()
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<QString>("normalised");

    // Output convention: upper-case field/square, lower-case subsquare.
    QTest::newRow("EM29jx") << "EM29jx" << "EM29jx";
    QTest::newRow("em29JX") << "em29JX" << "EM29jx";
    QTest::newRow("FN31pr") << "FN31pr" << "FN31pr";
    QTest::newRow("JO62qm") << "JO62qm" << "JO62qm";
    QTest::newRow("QF56od") << "QF56od" << "QF56od";
    QTest::newRow("GG66rw") << "GG66rw" << "GG66rw";
}

void TestMaidenhead::roundTrip()
{
    QFETCH(QString, input);
    QFETCH(QString, normalised);

    double lat = 0.0, lon = 0.0;
    QVERIFY(Maidenhead::toLatLon(input, lat, lon));
    QCOMPARE(Maidenhead::fromLatLon(lat, lon, 6), normalised);
    QCOMPARE(Maidenhead::fromLatLon(lat, lon, 4), normalised.left(4));
}

void TestMaidenhead::haversineProperties()
{
    const double halfCircumference = kEarthRadiusKm * std::numbers::pi;

    QCOMPARE(Maidenhead::distanceKm(39.5, -95.0, 39.5, -95.0), 0.0);
    QVERIFY(std::abs(Maidenhead::distanceKm(0, 0, 0, 90) - halfCircumference / 2) < 1e-6);
    QVERIFY(std::abs(Maidenhead::distanceKm(0, 0, 0, 180) - halfCircumference) < 1e-6);
    QVERIFY(std::abs(Maidenhead::distanceKm(90, 0, -90, 0) - halfCircumference) < 1e-6);
    // Crossing the antimeridian takes the short way round.
    QVERIFY(std::abs(Maidenhead::distanceKm(0, 179, 0, -179)
                     - halfCircumference * 2.0 / 180.0) < 1e-6);

    const double ab = Maidenhead::distanceKm(39.5, -95.0, 48.1375, 11.575);
    const double ba = Maidenhead::distanceKm(48.1375, 11.575, 39.5, -95.0);
    QCOMPARE(ab, ba);
}

void TestMaidenhead::gridDistances()
{
    const auto same = Maidenhead::distanceKm(QStringLiteral("EM29"), QStringLiteral("em29"));
    QVERIFY(same.has_value());
    QCOMPARE(*same, 0.0);

    // Grid-to-grid uses the cell centres.
    const auto g = Maidenhead::distanceKm(QStringLiteral("JJ00"), QStringLiteral("JJ90"));
    QVERIFY(g.has_value());
    QVERIFY(std::abs(*g - Maidenhead::distanceKm(0.5, 1.0, 0.5, 19.0)) < 1e-9);

    const auto gl = Maidenhead::distanceKm(QStringLiteral("EM29"), 39.5, -95.0);
    QVERIFY(gl.has_value());
    QCOMPARE(*gl, 0.0);

    QVERIFY(!Maidenhead::distanceKm(QStringLiteral("EM29"), QStringLiteral("ZZ")).has_value());
    QVERIFY(!Maidenhead::distanceKm(QStringLiteral("XX"), 0.0, 0.0).has_value());
}

QTEST_GUILESS_MAIN(TestMaidenhead)
#include "test_maidenhead.moc"
