// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ryan Butler (NF0T)
//
// Characterization tests: pin AdifParser / AdifWriter behaviour before they
// move into nf0t-core. QEXPECT_FAIL marks a known defect that A1 records but
// does not fix (no production changes in this step); fixing it turns the
// expected failure into an XPASS, which fails the test and forces the
// marker to be removed.

#include <QTemporaryDir>
#include <QtTest>

#include "QsoFixtures.h"
#include "core/adif/AdifParser.h"
#include "core/adif/AdifWriter.h"

class TestAdif : public QObject
{
    Q_OBJECT

private slots:
    // Writer → parser
    void stringRoundTripIsLossless();
    void fileRoundTripIsLossless();
    void defaultsAreOmittedAndRoundTrip();
    void arrlCheckRoundTrip();
    void appFieldsCanBeExcluded();
    void headerContents();

    // Parser
    void lengthBasedTokenising();
    void fieldNamesAreCaseInsensitive();
    void headerFieldsAreNotRecords();
    void headerlessFileKeepsFirstRecord();
    void writerHeaderlessOutputIsNotReadBack();
    void recordsMissingRequiredFieldsAreSkipped();
    void fourDigitTimeOn();
    void unparseableDateWarns();
    void qslStatusNormalisation();
    void latLonFormat();
    void lengthsAreUtf16CodeUnits();
};

namespace {
constexpr double kLatLonTol = 1e-5;   // ADIF stores minutes to 3 decimals

Qso minimal(const QString &call = QStringLiteral("W1AW"))
{
    Qso q;
    q.callsign   = call;
    q.datetimeOn = QDateTime(QDate(2026, 1, 2), QTime(3, 4, 5), QTimeZone::utc());
    q.band       = QStringLiteral("40m");
    q.freq       = 7.074;
    q.mode       = QStringLiteral("FT8");
    return q;
}
} // namespace

// --- Writer → parser ---------------------------------------------------------

void TestAdif::stringRoundTripIsLossless()
{
    const Qso in = QsoFixtures::fullyPopulated();
    const auto r = AdifParser::parseString(AdifWriter::writeString({in}));
    QCOMPARE(r.qsos.size(), 1);
    QCOMPARE(r.skipped, 0);
    QVERIFY2(r.warnings.isEmpty(), qPrintable(r.warnings.join('\n')));
    COMPARE_QSO(r.qsos.first(), in, kLatLonTol);
    QCOMPARE(r.qsos.first().datetimeOn.offsetFromUtc(), 0);
}

void TestAdif::fileRoundTripIsLossless()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("log.adi"));

    const QList<Qso> in{QsoFixtures::fullyPopulated(), minimal(QStringLiteral("JA1XYZ"))};
    QVERIFY(AdifWriter::writeFile(path, in));

    const auto r = AdifParser::parseFile(path);
    QCOMPARE(r.qsos.size(), 2);
    COMPARE_QSO(r.qsos.at(0), in.at(0), kLatLonTol);
    COMPARE_QSO(r.qsos.at(1), in.at(1), kLatLonTol);
}

void TestAdif::defaultsAreOmittedAndRoundTrip()
{
    const Qso in = minimal();
    const QString text = AdifWriter::writeString({in});

    // Unset optionals, 'N' QSL statuses, zero ints and empty strings are omitted.
    for (const char *absent : {"FREQ_RX", "LOTW_QSL_SENT", "DXCC", "CQZ", "NAME", "TX_PWR",
                               "LAT", "AGE", "SRX", "QSO_DATE_OFF", "APP_NF0T_QRZ_QSL_SENT"})
        QVERIFY2(!text.contains(QStringLiteral("<%1:").arg(QLatin1String(absent))), absent);

    const auto r = AdifParser::parseString(text);
    QCOMPARE(r.qsos.size(), 1);
    COMPARE_QSO(r.qsos.first(), in, 0.0);
}

void TestAdif::arrlCheckRoundTrip()
{
    // ADIF's Sweepstakes check field is CHECK. The writer emits arrlCheck as
    // a second ARRL_SECT and the parser reads arrlCheck from ARRL_SECT, so the
    // check value overwrites the section on re-import.
    Qso in = minimal();
    in.arrlSect  = QStringLiteral("EMO");
    in.arrlCheck = QStringLiteral("75");

    const QString text = AdifWriter::writeString({in});
    QVERIFY(text.contains(QStringLiteral("<ARRL_SECT:3>EMO")));
    QVERIFY(text.contains(QStringLiteral("<ARRL_SECT:2>75")));
    QVERIFY(!text.contains(QStringLiteral("<CHECK:")));

    const auto r = AdifParser::parseString(text);
    QCOMPARE(r.qsos.size(), 1);
    QCOMPARE(r.qsos.first().arrlCheck, in.arrlCheck);
    QEXPECT_FAIL("", "Known defect: arrlCheck is written and read as ARRL_SECT, overwriting the section", Continue);
    QCOMPARE(r.qsos.first().arrlSect, in.arrlSect);
}

void TestAdif::appFieldsCanBeExcluded()
{
    const Qso in = QsoFixtures::fullyPopulated();
    AdifWriter::Options opts;

    QVERIFY(AdifWriter::writeString({in}, opts).contains(QStringLiteral("<APP_NF0T_QRZ_QSL_SENT:1>Y")));

    opts.includeAppFields = false;
    const QString text = AdifWriter::writeString({in}, opts);
    QVERIFY(!text.contains(QStringLiteral("APP_NF0T_")));

    const auto r = AdifParser::parseString(text);
    QCOMPARE(r.qsos.size(), 1);
    QCOMPARE(r.qsos.first().qrzQslSent, QChar('N'));
    QCOMPARE(r.qsos.first().clublogQslSent, QChar('N'));
    QVERIFY(!r.qsos.first().qrzSentDate.isValid());
    QCOMPARE(r.qsos.first().lotwQslSent, QChar('Y'));   // standard fields unaffected
}

void TestAdif::headerContents()
{
    AdifWriter::Options opts;
    opts.programId      = QStringLiteral("TestProg");
    opts.programVersion = QStringLiteral("1.2.3");
    const QString text = AdifWriter::writeString({}, opts);

    QVERIFY(text.contains(QStringLiteral("<ADIF_VER:5>3.1.4")));
    QVERIFY(text.contains(QStringLiteral("<PROGRAMID:8>TestProg")));
    QVERIFY(text.contains(QStringLiteral("<PROGRAMVERSION:5>1.2.3")));
    QVERIFY(text.contains(QStringLiteral("<CREATED_TIMESTAMP:15>")));
    QVERIFY(text.contains(QStringLiteral("<EOH>")));
    QVERIFY(!text.contains(QStringLiteral("<EOR>")));

    QCOMPARE(AdifWriterOptions().programVersion, QStringLiteral(APP_VERSION));
}

// --- Parser ------------------------------------------------------------------

void TestAdif::lengthBasedTokenising()
{
    // The value is taken by length, so '<', '>' and a fake tag inside it are
    // data. Trailing text after the value up to the next '<' is ignored.
    const QString text = QStringLiteral(
        "<EOH>"
        "<CALL:4>W1AW <QSO_DATE:8>20260102 <TIME_ON:6>030405"
        "<COMMENT:19>a<b> <EOR> c:d,e;f trailing junk"
        "<NOTES:0><EOR>");
    const auto r = AdifParser::parseString(text);
    QCOMPARE(r.qsos.size(), 1);
    QCOMPARE(r.qsos.first().callsign, QStringLiteral("W1AW"));
    QCOMPARE(r.qsos.first().comment, QStringLiteral("a<b> <EOR> c:d,e;f "));
    QVERIFY(r.qsos.first().notes.isEmpty());
}

void TestAdif::fieldNamesAreCaseInsensitive()
{
    const auto r = AdifParser::parseString(QStringLiteral(
        "<eoh><call:4>w1aw<Qso_Date:8>20260102<time_on:4>0304<band:3>40M<mode:3>ft8<eor>"));
    QCOMPARE(r.qsos.size(), 1);
    const Qso &q = r.qsos.first();
    QCOMPARE(q.callsign, QStringLiteral("W1AW"));   // upper-cased
    QCOMPARE(q.band, QStringLiteral("40m"));        // lower-cased
    QCOMPARE(q.mode, QStringLiteral("FT8"));        // upper-cased
}

void TestAdif::headerFieldsAreNotRecords()
{
    const auto r = AdifParser::parseString(QStringLiteral(
        "Free text preamble\n<ADIF_VER:5>3.1.4 <PROGRAMID:4>Test <CALL:4>XXXX\n<EOH>\n"
        "<CALL:4>W1AW<QSO_DATE:8>20260102<TIME_ON:6>030405<EOR>"));
    QCOMPARE(r.qsos.size(), 1);
    QCOMPARE(r.qsos.first().callsign, QStringLiteral("W1AW"));
}

void TestAdif::headerlessFileKeepsFirstRecord()
{
    // ADIF allows a file with no header. The parser's own comment says such
    // files are processed, but fields are only collected after <EOH> or the
    // first <EOR>, so the first record of a headerless file is dropped. This
    // includes AdifWriter's own output with includeHeader = false.
    const auto r = AdifParser::parseString(QStringLiteral(
        "<CALL:4>W1AW<QSO_DATE:8>20260102<TIME_ON:6>030405<EOR>"
        "<CALL:5>K1ABC<QSO_DATE:8>20260102<TIME_ON:6>030506<EOR>"));
    QEXPECT_FAIL("", "Known defect: first record of a headerless ADIF file is silently dropped", Abort);
    QCOMPARE(r.qsos.size(), 2);
}

void TestAdif::writerHeaderlessOutputIsNotReadBack()
{
    AdifWriter::Options opts;
    opts.includeHeader = false;
    const QString text = AdifWriter::writeString({minimal()}, opts);
    QVERIFY(!text.contains(QStringLiteral("<EOH>")));

    const auto r = AdifParser::parseString(text);
    QEXPECT_FAIL("", "Known defect: first record of a headerless ADIF file is silently dropped", Abort);
    QCOMPARE(r.qsos.size(), 1);
}

void TestAdif::recordsMissingRequiredFieldsAreSkipped()
{
    const auto r = AdifParser::parseString(QStringLiteral(
        "<EOH>"
        "<CALL:4>W1AW<QSO_DATE:8>20260102<EOR>"                      // no TIME_ON
        "<QSO_DATE:8>20260102<TIME_ON:6>030405<EOR>"                 // no CALL
        "<CALL:5>K1ABC<QSO_DATE:8>20260102<TIME_ON:6>030506<EOR>"
        "<EOR>"));                                                   // empty: ignored
    QCOMPARE(r.qsos.size(), 1);
    QCOMPARE(r.skipped, 2);
    QCOMPARE(r.warnings.size(), 2);
    QCOMPARE(r.qsos.first().callsign, QStringLiteral("K1ABC"));
}

void TestAdif::fourDigitTimeOn()
{
    const auto r = AdifParser::parseString(QStringLiteral(
        "<EOH><CALL:4>W1AW<QSO_DATE:8>20260102<TIME_ON:4>2359<EOR>"));
    QCOMPARE(r.qsos.size(), 1);
    QCOMPARE(r.qsos.first().datetimeOn,
             QDateTime(QDate(2026, 1, 2), QTime(23, 59, 0), QTimeZone::utc()));
}

void TestAdif::unparseableDateWarns()
{
    // Present but unparseable: the record is kept, with an invalid datetime.
    const auto r = AdifParser::parseString(QStringLiteral(
        "<EOH><CALL:4>W1AW<QSO_DATE:8>2026XX02<TIME_ON:6>030405<EOR>"));
    QCOMPARE(r.qsos.size(), 1);
    QCOMPARE(r.skipped, 0);
    QVERIFY(!r.qsos.first().datetimeOn.isValid());
    QCOMPARE(r.warnings.size(), 1);
}

void TestAdif::qslStatusNormalisation()
{
    const auto r = AdifParser::parseString(QStringLiteral(
        "<EOH><CALL:4>W1AW<QSO_DATE:8>20260102<TIME_ON:6>030405"
        "<LOTW_QSL_SENT:1>y<LOTW_QSL_RCVD:1>X<EQSL_QSL_SENT:3>Rxx<EQSL_QSL_RCVD:0>"
        "<QSL_SENT:1>i<EOR>"));
    QCOMPARE(r.qsos.size(), 1);
    const Qso &q = r.qsos.first();
    QCOMPARE(q.lotwQslSent, QChar('Y'));   // case folded
    QCOMPARE(q.lotwQslRcvd, QChar('N'));   // not one of Y N R Q I
    QCOMPARE(q.eqslQslSent, QChar('R'));   // first character only
    QCOMPARE(q.eqslQslRcvd, QChar('N'));   // empty
    QCOMPARE(q.qslSent,     QChar('I'));
    QCOMPARE(q.qslRcvd,     QChar('N'));   // absent
}

void TestAdif::latLonFormat()
{
    Qso in = minimal();
    in.lat = -33.8688;
    in.lon = 151.2093;
    const QString text = AdifWriter::writeString({in});
    QVERIFY(text.contains(QStringLiteral("<LAT:11>S033 52.128")));
    QVERIFY(text.contains(QStringLiteral("<LON:11>E151 12.558")));

    const auto r = AdifParser::parseString(QStringLiteral(
        "<EOH><CALL:4>W1AW<QSO_DATE:8>20260102<TIME_ON:6>030405"
        "<LAT:11>N039 58.750<LON:11>W095 12.500<MY_LAT:3>bad<EOR>"));
    QCOMPARE(r.qsos.size(), 1);
    const Qso &q = r.qsos.first();
    QVERIFY(q.lat.has_value() && q.lon.has_value());
    QVERIFY(std::abs(*q.lat - 39.9791666667) < 1e-9);
    QVERIFY(std::abs(*q.lon - -95.2083333333) < 1e-9);
    QVERIFY(!q.myLat.has_value());
}

void TestAdif::lengthsAreUtf16CodeUnits()
{
    // Field lengths are QString lengths (UTF-16 code units) on both the write
    // and the read side, so the logger round-trips its own files. A file from
    // a program that counts UTF-8 bytes is read differently.
    Qso in = minimal();
    in.name = QStringLiteral("José");
    in.comment = QString::fromUtf8("73 \xF0\x9F\x93\xBB");   // non-BMP: 2 code units
    const QString text = AdifWriter::writeString({in});
    QVERIFY(text.contains(QStringLiteral("<NAME:4>José")));
    QVERIFY(text.contains(QStringLiteral("<COMMENT:5>")));

    const auto own = AdifParser::parseString(text);
    QCOMPARE(own.qsos.size(), 1);
    QCOMPARE(own.qsos.first().name, in.name);
    QCOMPARE(own.qsos.first().comment, in.comment);

    const auto bytes = AdifParser::parseString(QStringLiteral(
        "<EOH><CALL:4>W1AW<QSO_DATE:8>20260102<TIME_ON:6>030405<NAME:5>José<EOR>"
        "<CALL:5>K1ABC<QSO_DATE:8>20260102<TIME_ON:6>030506<EOR>"));
    // The over-long NAME swallows the first <EOR>'s '<', so the two records
    // merge into one carrying the second record's call.
    QCOMPARE(bytes.qsos.size(), 1);
    QCOMPARE(bytes.qsos.first().name, QStringLiteral("José<"));
    QCOMPARE(bytes.qsos.first().callsign, QStringLiteral("K1ABC"));
}

QTEST_GUILESS_MAIN(TestAdif)
#include "test_adif.moc"
