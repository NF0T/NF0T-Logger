// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ryan Butler (NF0T)
//
// Shared fixtures for the core characterization tests.
#pragma once

#include <cmath>
#include <optional>

#include <QTimeZone>
#include <QtTest>

#include "core/logbook/Qso.h"

namespace QsoFixtures {

/// Every Qso field populated with a value that survives ADIF's fixed
/// precision (FREQ 6 decimals, most other reals 1 decimal, lat/lon to
/// 0.001 minute). Strings are in the case the ADIF parser normalises to.
inline Qso fullyPopulated()
{
    const QTimeZone utc = QTimeZone::utc();
    Qso q;
    q.callsign    = QStringLiteral("DL1ABC");
    q.datetimeOn  = QDateTime(QDate(2026, 11, 16), QTime(14, 23, 45), utc);
    q.datetimeOff = QDateTime(QDate(2026, 11, 16), QTime(14, 31, 2), utc);
    q.band        = QStringLiteral("20m");
    q.freq        = 14.074123;
    q.mode        = QStringLiteral("MFSK");
    q.submode     = QStringLiteral("FT4");
    q.freqRx      = 14.076500;
    q.bandRx      = QStringLiteral("20m");

    q.rstSent = QStringLiteral("-10");
    q.rstRcvd = QStringLiteral("+03");

    q.name        = QStringLiteral("Jürgen Müller");      // multibyte UTF-8
    q.contactedOp = QStringLiteral("DL2XYZ");
    q.qth         = QStringLiteral("München <Bayern>");   // angle brackets in a value
    q.gridsquare  = QStringLiteral("JN58TD");
    q.country     = QStringLiteral("Fed. Rep. of Germany");
    q.state       = QStringLiteral("BY");
    q.county      = QStringLiteral("Oberbayern");
    q.cont        = QStringLiteral("EU");
    q.dxcc        = 230;
    q.cqZone      = 14;
    q.ituZone     = 28;
    q.pfx         = QStringLiteral("DL1");
    q.arrlSect    = QStringLiteral("DX");
    q.lat         = 48.137500;
    q.lon         = 11.575000;
    q.distance    = 7654.3;
    q.age         = 42;
    q.eqCall      = QStringLiteral("DL1ABC/P");

    q.stationCallsign = QStringLiteral("NF0T");
    q.operatorCall    = QStringLiteral("NF0T");
    q.ownerCallsign   = QStringLiteral("NF0T");
    q.myName          = QStringLiteral("Test Operator");
    q.myGridsquare    = QStringLiteral("EM29JX");
    q.myCity          = QStringLiteral("Test City");
    q.myState         = QStringLiteral("MO");
    q.myCounty        = QStringLiteral("Jackson");
    q.myCountry       = QStringLiteral("United States");
    q.myDxcc          = 291;
    q.myCqZone        = 4;
    q.myItuZone       = 7;
    q.myLat           = 39.979167;
    q.myLon           = -95.208333;
    q.txPwr           = 100.0;
    q.rxPwr           = 5.0;
    q.antenna         = QStringLiteral("Hex beam");
    q.antAz           = 45.0;
    q.antEl           = 10.0;
    q.antPath         = QStringLiteral("S");
    q.rig             = QStringLiteral("FLEX-8400");

    q.lotwQslSent  = 'Y';  q.lotwQslRcvd  = 'Y';
    q.lotwSentDate = QDate(2026, 11, 17);  q.lotwRcvdDate = QDate(2026, 11, 20);
    q.eqslQslSent  = 'Y';  q.eqslQslRcvd  = 'R';
    q.eqslSentDate = QDate(2026, 11, 17);  q.eqslRcvdDate = QDate(2026, 11, 21);
    q.qrzQslSent   = 'Y';  q.qrzQslRcvd   = 'Y';
    q.qrzSentDate  = QDate(2026, 11, 18);  q.qrzRcvdDate  = QDate(2026, 11, 22);
    q.clublogQslSent  = 'Y';
    q.clublogSentDate = QDate(2026, 11, 19);
    q.qslSent      = 'Q';  q.qslRcvd      = 'I';
    q.qslSentDate  = QDate(2026, 12, 1);   q.qslRcvdDate  = QDate(2026, 12, 15);
    q.qslVia       = QStringLiteral("DARC bureau");
    q.qslMsg       = QStringLiteral("Tnx QSO, 73");

    q.propMode = QStringLiteral("F2");
    q.sfi      = 150.0;
    q.aIndex   = 5.0;
    q.kIndex   = 2.0;
    q.sunspots = 120;

    q.contestId    = QStringLiteral("CQ-WW-DX");
    q.srx          = 42;
    q.srxString    = QStringLiteral("042 14");
    q.stx          = 7;
    q.stxString    = QStringLiteral("007 4");
    q.precedence   = QStringLiteral("A");
    q.contestClass = QStringLiteral("3A");
    q.arrlCheck    = QStringLiteral("DX");   // must equal arrlSect: see TestAdif::arrlCheckRoundTrip

    q.sig       = QStringLiteral("POTA");
    q.sigInfo   = QStringLiteral("DE-0001");
    q.mySig     = QStringLiteral("POTA");
    q.mySigInfo = QStringLiteral("US-0001");

    q.satName   = QStringLiteral("QO-100");
    q.satMode   = QStringLiteral("SX");
    q.maxBursts = 3.0;
    q.nrBursts  = 4;
    q.nrPings   = 5;
    q.msShower  = QStringLiteral("Geminids");

    q.comment     = QStringLiteral("Comment with a : colon and , comma");
    q.notes       = QStringLiteral("Line one\nLine two");
    q.qsoComplete = QStringLiteral("Y");
    return q;
}

inline bool closeEnough(const std::optional<double> &a, const std::optional<double> &b, double tol)
{
    if (a.has_value() != b.has_value()) return false;
    return !a.has_value() || std::abs(*a - *b) <= tol;
}

/// Field-by-field comparison. Lat/lon fields get latLonTol; every other real
/// must match exactly. Does not compare id. Call through COMPARE_QSO so a
/// failure stops the calling test.
inline void compareQso(const Qso &a, const Qso &b, double latLonTol)
{
#define CMP(f) QCOMPARE(a.f, b.f)
#define CMP_LL(f) QVERIFY2(closeEnough(a.f, b.f, latLonTol), #f)
    CMP(callsign); CMP(datetimeOn); CMP(datetimeOff); CMP(band); CMP(freq); CMP(mode);
    CMP(submode); CMP(freqRx); CMP(bandRx);
    CMP(rstSent); CMP(rstRcvd);
    CMP(name); CMP(contactedOp); CMP(qth); CMP(gridsquare); CMP(country); CMP(state);
    CMP(county); CMP(cont); CMP(dxcc); CMP(cqZone); CMP(ituZone); CMP(pfx); CMP(arrlSect);
    CMP_LL(lat); CMP_LL(lon); CMP(distance); CMP(age); CMP(eqCall);
    CMP(stationCallsign); CMP(operatorCall); CMP(ownerCallsign); CMP(myName);
    CMP(myGridsquare); CMP(myCity); CMP(myState); CMP(myCounty); CMP(myCountry);
    CMP(myDxcc); CMP(myCqZone); CMP(myItuZone); CMP_LL(myLat); CMP_LL(myLon);
    CMP(txPwr); CMP(rxPwr); CMP(antenna); CMP(antAz); CMP(antEl); CMP(antPath); CMP(rig);
    CMP(lotwQslSent); CMP(lotwQslRcvd); CMP(lotwSentDate); CMP(lotwRcvdDate);
    CMP(eqslQslSent); CMP(eqslQslRcvd); CMP(eqslSentDate); CMP(eqslRcvdDate);
    CMP(qrzQslSent); CMP(qrzQslRcvd); CMP(qrzSentDate); CMP(qrzRcvdDate);
    CMP(clublogQslSent); CMP(clublogSentDate);
    CMP(qslSent); CMP(qslRcvd); CMP(qslSentDate); CMP(qslRcvdDate); CMP(qslVia); CMP(qslMsg);
    CMP(propMode); CMP(sfi); CMP(aIndex); CMP(kIndex); CMP(sunspots);
    CMP(contestId); CMP(srx); CMP(srxString); CMP(stx); CMP(stxString); CMP(precedence);
    CMP(contestClass); CMP(arrlCheck);
    CMP(sig); CMP(sigInfo); CMP(mySig); CMP(mySigInfo);
    CMP(satName); CMP(satMode); CMP(maxBursts); CMP(nrBursts); CMP(nrPings); CMP(msShower);
    CMP(comment); CMP(notes); CMP(qsoComplete);
#undef CMP
#undef CMP_LL
}

} // namespace QsoFixtures

#define COMPARE_QSO(a, b, tol)                                   \
    do {                                                          \
        QsoFixtures::compareQso((a), (b), (tol));                 \
        if (QTest::currentTestFailed()) return;                   \
    } while (false)
