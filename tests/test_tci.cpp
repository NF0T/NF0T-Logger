// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ryan Butler (NF0T)
//
// TciBackend against the TCI 2.1 specification. Section references (§) are to
// "TCI Protocol 2.1", Expert Electronics, September 2026.

#include <QSignalSpy>
#include <QtTest>

#include "radio/TciBackend.h"

class TestTciBackend : public QObject
{
    Q_OBJECT

private slots:
    // Initialization and format (§3.1, §4.1, §4.7)
    void handshakeIsParsed();
    void commandNamesAreCaseInsensitive();
    void legacyChannelCountIsAccepted();
    void emptyArgumentsAreDiscarded();
    void malformedNumbersAreIgnored();
    void booleanRule();

    // Transceiver focus
    void focusFollowsActiveSlice();
    void focusFollowsFirstEcoderPanelOnly();
    void focusIgnoresOutOfRangeTrx();
    void focusFollowsSliceAnnouncedAfterConnect();
    void splitAppliesOnlyWhileFocusedOnSplitTrx();
    void disabledTrxCannotHaveFocus();

    // Outbound commands
    void outboundTargetsActiveTrx();
    void nothingIsSentBeforeReady();
    void outboundFrequencyIsIntegerHz();
    void outboundModeHonoursModulationsList();

    // TX, split, spots
    void transmitIsTrueWhileAnyTrxTransmits();
    void trxAcceptsSignalSourceAndIgnoresReadForm();
    void vfoBIsTxFrequencyOnlyWhileSplit();
    void txFrequencyNotificationAppliesOnlyWhileSplit();
    void disconnectClearsTxAndSplit();
    void spotClickReportedOnceWhenBothFormsArrive();
    void distinctSpotClicksAreAllReported();

    void modeConversions();

private:
    // Sets up a connected backend that has received the init burst.
    static void start(TciBackend &tci, int trxCount = 2,
                      const QString &modulations = QStringLiteral("usb,lsb,cw,cwr,am,nfm,digu,digl"))
    {
        tci.onConnected();
        feed(tci, QStringLiteral("protocol:ExpertSDR3,2.1;device:TestRig;receive_only:false;"
                                 "trx_count:%1;channels_count:2;modulations_list:%2;ready;")
                      .arg(trxCount).arg(modulations));
    }
    static void feed(TciBackend &tci, const QString &msg) { tci.onTextMessageReceived(msg); }
    static void capture(TciBackend &tci, QStringList &sent)
    {
        tci.m_sender = [&sent](const QString &cmd) { sent << cmd; };
    }
};

// --- Initialization and format ---------------------------------------------

void TestTciBackend::handshakeIsParsed()
{
    TciBackend tci;
    QSignalSpy devSpy(&tci, &RadioBackend::deviceDiscovered);
    start(tci, 4);

    QCOMPARE(tci.protocolVersion(), QStringLiteral("ExpertSDR3,2.1"));
    QCOMPARE(tci.deviceName(), QStringLiteral("TestRig"));
    QCOMPARE(tci.trxCount(), 4);
    QVERIFY(!tci.isReceiveOnly());
    QCOMPARE(tci.supportedModulations().first(), QStringLiteral("USB"));
    QCOMPARE(devSpy.count(), 1);
    QCOMPARE(devSpy.at(0).at(0).toString(), QStringLiteral("TestRig"));

    feed(tci, "receive_only:true;");
    QVERIFY(tci.isReceiveOnly());
}

void TestTciBackend::commandNamesAreCaseInsensitive()
{
    TciBackend tci;
    start(tci);
    QSignalSpy freqSpy(&tci, &RadioBackend::freqChanged);
    QSignalSpy txSpy(&tci, &RadioBackend::transmitChanged);

    feed(tci, "VFO:0,0,7100000;Trx:0,TRUE;");
    QCOMPARE(freqSpy.count(), 1);
    QCOMPARE(freqSpy.at(0).at(0).toDouble(), 7.1);
    QCOMPARE(txSpy.count(), 1);
    QVERIFY(txSpy.at(0).at(0).toBool());
}

void TestTciBackend::legacyChannelCountIsAccepted()
{
    TciBackend tci;
    tci.onConnected();
    feed(tci, "channel_count:2;trx_count:2;ready;");
    QCOMPARE(tci.m_channelsCount, 2);
}

void TestTciBackend::emptyArgumentsAreDiscarded()
{
    // §4.7 note 1, the spec's own example: VFO:0,,7100000; parses as the
    // two-argument read form and must not be taken as a frequency.
    TciBackend tci;
    start(tci);
    QSignalSpy freqSpy(&tci, &RadioBackend::freqChanged);

    feed(tci, "vfo:0,,7100000;");
    QCOMPARE(freqSpy.count(), 0);

    feed(tci, "vfo: 0 , 0 , 7100000 ;");
    QCOMPARE(freqSpy.count(), 1);
}

void TestTciBackend::malformedNumbersAreIgnored()
{
    TciBackend tci;
    start(tci);
    QSignalSpy freqSpy(&tci, &RadioBackend::freqChanged);
    QSignalSpy txSpy(&tci, &RadioBackend::transmitChanged);

    feed(tci, "vfo:x,0,7100000;vfo:0,0,abc;vfo:0,0,-7100000;vfo:-1,0,7100000;trx:a,true;");
    QCOMPARE(freqSpy.count(), 0);
    QCOMPARE(txSpy.count(), 0);
}

void TestTciBackend::booleanRule()
{
    // §4.7 note 3: true if "true" or "1" in any case; anything else is false.
    TciBackend tci;
    start(tci);
    QSignalSpy txSpy(&tci, &RadioBackend::transmitChanged);

    feed(tci, "trx:0,1;");      QCOMPARE(txSpy.count(), 1); QVERIFY(txSpy.last().at(0).toBool());
    feed(tci, "trx:0,0;");      QCOMPARE(txSpy.count(), 2); QVERIFY(!txSpy.last().at(0).toBool());
    feed(tci, "trx:0,True;");   QCOMPARE(txSpy.count(), 3); QVERIFY(txSpy.last().at(0).toBool());
    feed(tci, "trx:0,yes;");    QCOMPARE(txSpy.count(), 4); QVERIFY(!txSpy.last().at(0).toBool());
}

// --- Transceiver focus -------------------------------------------------------

void TestTciBackend::focusFollowsActiveSlice()
{
    TciBackend tci;
    start(tci);
    QSignalSpy freqSpy(&tci, &RadioBackend::freqChanged);
    QSignalSpy modeSpy(&tci, &RadioBackend::modeChanged);

    feed(tci, "vfo:0,0,14074000;modulation:0,DIGU;vfo:1,0,7030000;modulation:1,CW;");
    QCOMPARE(tci.activeTrx(), 0);
    QCOMPARE(freqSpy.count(), 1);                       // TRX 1 is not focused
    QCOMPARE(freqSpy.last().at(0).toDouble(), 14.074);

    feed(tci, "active_slice:1,B;");
    QCOMPARE(tci.activeTrx(), 1);
    QCOMPARE(freqSpy.last().at(0).toDouble(), 7.03);
    QCOMPARE(modeSpy.last().at(0).toString(), QStringLiteral("CW"));
}

void TestTciBackend::focusFollowsFirstEcoderPanelOnly()
{
    TciBackend tci;
    start(tci);

    feed(tci, "ecoder_switch_rx:1,1;");
    QCOMPARE(tci.activeTrx(), 0);
    feed(tci, "ecoder_switch_rx:0,1;");
    QCOMPARE(tci.activeTrx(), 1);
    feed(tci, "ecoder_switch_rx:0;");                    // read form
    QCOMPARE(tci.activeTrx(), 1);
}

void TestTciBackend::focusIgnoresOutOfRangeTrx()
{
    TciBackend tci;
    start(tci, 2);
    feed(tci, "active_slice:2,C;");
    QCOMPARE(tci.activeTrx(), 0);
}

void TestTciBackend::focusFollowsSliceAnnouncedAfterConnect()
{
    // AetherSDR sends TRX_COUNT once, at connect; a slice opened later is
    // announced with RX_ENABLE and must still be followable.
    TciBackend tci;
    start(tci, 1);
    feed(tci, "rx_enable:1,true;vfo:1,0,7030000;active_slice:1,B;");
    QCOMPARE(tci.activeTrx(), 1);

    feed(tci, "rx_enable:1,false;");                     // slice closed
    QCOMPARE(tci.activeTrx(), 0);
}

void TestTciBackend::splitAppliesOnlyWhileFocusedOnSplitTrx()
{
    // Order captured from AetherSDR (FLEX-8400): opening slice B focuses it,
    // then split is reported on A because B now holds TX. AetherSDR cannot
    // say whether the operator is working B alone or listening on A, so the
    // logger follows focus.
    TciBackend tci;
    start(tci, 1);
    feed(tci, "vfo:0,0,14218000;split_enable:0,false;");
    QSignalSpy freqSpy(&tci, &RadioBackend::freqChanged);
    QSignalSpy txFreqSpy(&tci, &RadioBackend::txFreqChanged);

    feed(tci, "rx_enable:1,true;active_slice:1,B;vfo:1,0,14240000;");
    feed(tci, "split_enable:0,true;vfo:0,1,14240000;");
    QCOMPARE(tci.activeTrx(), 1);
    QVERIFY(!tci.isSplit());                             // working B: not split
    QVERIFY(!freqSpy.isEmpty());
    QCOMPARE(freqSpy.last().at(0).toDouble(), 14.24);
    QCOMPARE(txFreqSpy.count(), 0);

    feed(tci, "active_slice:0,A;");                      // listening on A, TX on B
    QCOMPARE(tci.activeTrx(), 0);
    QVERIFY(tci.isSplit());
    QCOMPARE(freqSpy.last().at(0).toDouble(), 14.218);
    QVERIFY(!txFreqSpy.isEmpty());
    QCOMPARE(txFreqSpy.last().at(0).toDouble(), 14.24);

    feed(tci, "split_enable:0,false;rx_enable:1,false;");
    QVERIFY(!tci.isSplit());
}

void TestTciBackend::disabledTrxCannotHaveFocus()
{
    TciBackend tci;
    start(tci, 3);

    feed(tci, "rx_enable:2,false;active_slice:2,C;");
    QCOMPARE(tci.activeTrx(), 0);

    feed(tci, "active_slice:1,B;rx_enable:1,false;");
    QCOMPARE(tci.activeTrx(), 0);                        // fell back to TRX 0

    feed(tci, "rx_enable:0,false;");                     // §4.5: TRX 0 is always enabled
    feed(tci, "active_slice:1,B;active_slice:0,A;");
    QCOMPARE(tci.activeTrx(), 0);
    QVERIFY(tci.m_trx.value(0).enabled);
}

// --- Outbound commands -------------------------------------------------------

void TestTciBackend::outboundTargetsActiveTrx()
{
    TciBackend tci;
    QStringList sent;
    capture(tci, sent);
    start(tci);

    feed(tci, "active_slice:1,B;");
    tci.setFreq(7.074);
    tci.setMode(QStringLiteral("CW"));
    QCOMPARE(sent, (QStringList{QStringLiteral("vfo:1,0,7074000;"),
                                QStringLiteral("modulation:1,CW;")}));
}

void TestTciBackend::nothingIsSentBeforeReady()
{
    TciBackend tci;
    QStringList sent;
    capture(tci, sent);
    tci.onConnected();
    feed(tci, "trx_count:2;");

    tci.setFreq(14.074);
    tci.setMode(QStringLiteral("SSB"), QStringLiteral("USB"));
    QVERIFY(sent.isEmpty());
}

void TestTciBackend::outboundFrequencyIsIntegerHz()
{
    // §4.7 notes 2 and 4: integer Hz; a value that fails to parse would turn the
    // set into a read, so zero and negative values are not sent at all.
    TciBackend tci;
    QStringList sent;
    capture(tci, sent);
    start(tci);

    tci.setFreq(14.0740004);
    tci.setFreq(14.0739996);
    tci.setFreq(0.0);
    tci.setFreq(-1.0);
    QCOMPARE(sent, (QStringList{QStringLiteral("vfo:0,0,14074000;"),
                                QStringLiteral("vfo:0,0,14074000;")}));
}

void TestTciBackend::outboundModeHonoursModulationsList()
{
    TciBackend tci;
    QStringList sent;
    capture(tci, sent);
    start(tci, 1, QStringLiteral("usb,lsb,cw,fm"));

    tci.setMode(QStringLiteral("FT8"));                         // DIGU not listed → USB
    tci.setMode(QStringLiteral("PSK"), QStringLiteral("LSB"));  // DIGL not listed → LSB
    tci.setMode(QStringLiteral("AM"));                          // no fallback → nothing
    tci.setMode(QStringLiteral("FM"));
    QCOMPARE(sent, (QStringList{QStringLiteral("modulation:0,USB;"),
                                QStringLiteral("modulation:0,LSB;"),
                                QStringLiteral("modulation:0,FM;")}));
}

// --- TX, split, spots --------------------------------------------------------

void TestTciBackend::transmitIsTrueWhileAnyTrxTransmits()
{
    // AetherSDR reports TRX against the TX slice, which can differ from focus.
    TciBackend tci;
    start(tci, 3);
    feed(tci, "active_slice:1,B;");
    QSignalSpy txSpy(&tci, &RadioBackend::transmitChanged);

    feed(tci, "trx:2,true;");
    QCOMPARE(txSpy.count(), 1);
    QVERIFY(txSpy.last().at(0).toBool());

    feed(tci, "trx:0,true;trx:2,false;");               // still transmitting on TRX 0
    QCOMPARE(txSpy.count(), 1);

    feed(tci, "trx:0,false;");
    QCOMPARE(txSpy.count(), 2);
    QVERIFY(!txSpy.last().at(0).toBool());
}

void TestTciBackend::trxAcceptsSignalSourceAndIgnoresReadForm()
{
    TciBackend tci;
    start(tci);
    QSignalSpy txSpy(&tci, &RadioBackend::transmitChanged);

    feed(tci, "trx:0,true,tci;");
    QCOMPARE(txSpy.count(), 1);
    feed(tci, "trx:0;");
    QCOMPARE(txSpy.count(), 1);
    QVERIFY(tci.m_transmitting);
}

void TestTciBackend::vfoBIsTxFrequencyOnlyWhileSplit()
{
    TciBackend tci;
    start(tci);
    QSignalSpy splitSpy(&tci, &RadioBackend::splitChanged);
    QSignalSpy txFreqSpy(&tci, &RadioBackend::txFreqChanged);

    feed(tci, "vfo:0,1,14074000;");                     // channel B as a second receiver
    QCOMPARE(txFreqSpy.count(), 0);

    feed(tci, "vfo:0,1,14085000;split_enable:0,true;"); // cached VFO B released on split
    QCOMPARE(splitSpy.count(), 1);
    QVERIFY(tci.isSplit());
    QCOMPARE(txFreqSpy.count(), 1);
    QCOMPARE(txFreqSpy.last().at(0).toDouble(), 14.085);

    feed(tci, "vfo:0,1,14086000;");
    QCOMPARE(txFreqSpy.last().at(0).toDouble(), 14.086);

    feed(tci, "split_enable:0,0;");
    QCOMPARE(splitSpy.count(), 2);
    QVERIFY(!tci.isSplit());
}

void TestTciBackend::txFrequencyNotificationAppliesOnlyWhileSplit()
{
    TciBackend tci;
    start(tci);
    QSignalSpy txFreqSpy(&tci, &RadioBackend::txFreqChanged);

    feed(tci, "tx_frequency:14074000;");
    QCOMPARE(txFreqSpy.count(), 0);

    feed(tci, "split_enable:0,true;tx_frequency:14080000;");
    QCOMPARE(txFreqSpy.count(), 1);
    QCOMPARE(txFreqSpy.last().at(0).toDouble(), 14.08);
}

void TestTciBackend::disconnectClearsTxAndSplit()
{
    TciBackend tci;
    start(tci);
    tci.m_intentionalClose = true;                      // no reconnect timer
    feed(tci, "split_enable:0,true;trx:0,true;");

    QSignalSpy txSpy(&tci, &RadioBackend::transmitChanged);
    QSignalSpy splitSpy(&tci, &RadioBackend::splitChanged);
    tci.onDisconnected();

    QCOMPARE(txSpy.count(), 1);
    QVERIFY(!txSpy.last().at(0).toBool());
    QCOMPARE(splitSpy.count(), 1);
    QVERIFY(!splitSpy.last().at(0).toBool());
}

void TestTciBackend::spotClickReportedOnceWhenBothFormsArrive()
{
    // AetherSDR broadcasts the legacy and receiver-qualified forms per click.
    TciBackend tci;
    start(tci);
    QSignalSpy spotSpy(&tci, &RadioBackend::spotClicked);

    feed(tci, "clicked_on_spot:k1abc,14025000;rx_clicked_on_spot:0,0,K1ABC,14025000;");
    QCOMPARE(spotSpy.count(), 1);
    QCOMPARE(spotSpy.at(0).at(0).toString(), QStringLiteral("K1ABC"));
    QCOMPARE(spotSpy.at(0).at(1).toDouble(), 14.025);
}

void TestTciBackend::distinctSpotClicksAreAllReported()
{
    TciBackend tci;
    start(tci);
    QSignalSpy spotSpy(&tci, &RadioBackend::spotClicked);

    feed(tci, "rx_clicked_on_spot:0,0,K1ABC,14025000;");
    feed(tci, "rx_clicked_on_spot:0,1,W1AW,7040000;");
    feed(tci, "clicked_on_spot:K1ABC,14026000;");
    QCOMPARE(spotSpy.count(), 3);
    QCOMPARE(spotSpy.at(1).at(0).toString(), QStringLiteral("W1AW"));
}

void TestTciBackend::modeConversions()
{
    TciBackend tci;
    start(tci);
    QSignalSpy modeSpy(&tci, &RadioBackend::modeChanged);

    auto check = [&](const char *tci_, const char *mode, const char *sub) {
        feed(tci, QStringLiteral("modulation:0,%1;").arg(QLatin1String(tci_)));
        QCOMPARE(modeSpy.last().at(0).toString(), QString::fromLatin1(mode));
        QCOMPARE(modeSpy.last().at(1).toString(), QString::fromLatin1(sub));
    };
    check("usb",  "SSB", "USB");
    check("LSB",  "SSB", "LSB");
    check("dsb",  "SSB", "");
    check("cwr",  "CW",  "");
    check("sam",  "AM",  "");
    check("nfm",  "FM",  "");
    check("digu", "SSB", "USB");
    check("digl", "SSB", "LSB");
    check("rtty", "RTTY", "");
}

QTEST_MAIN(TestTciBackend)
#include "test_tci.moc"
