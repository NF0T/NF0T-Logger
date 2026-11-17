// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ryan Butler (NF0T)
//
// QrzXmlLookupProvider availability. No network: lookups are not exercised.

#include <QtTest>

#include "lookup/QrzXmlLookupProvider.h"

class TestQrzXml : public QObject
{
    Q_OBJECT

private slots:
    void unsetConfigIsUnavailable();
    void availability_data();
    void availability();
    void configIsReadOnEveryCall();
};

void TestQrzXml::unsetConfigIsUnavailable()
{
    QrzXmlLookupProvider p{QrzXmlConfig{}};
    QVERIFY(!p.isAvailable());
}

void TestQrzXml::availability_data()
{
    QTest::addColumn<bool>("enabled");
    QTest::addColumn<QString>("user");
    QTest::addColumn<QString>("pass");
    QTest::addColumn<bool>("available");

    QTest::newRow("all set")       << true  << "N0CALL" << "secret" << true;
    QTest::newRow("disabled")      << false << "N0CALL" << "secret" << false;
    QTest::newRow("no username")   << true  << ""       << "secret" << false;
    QTest::newRow("no password")   << true  << "N0CALL" << ""       << false;
}

void TestQrzXml::availability()
{
    QFETCH(bool, enabled);
    QFETCH(QString, user);
    QFETCH(QString, pass);
    QFETCH(bool, available);

    QrzXmlLookupProvider p{QrzXmlConfig{
        .enabled  = [=] { return enabled; },
        .username = [=] { return user; },
        .password = [=] { return pass; },
    }};
    QCOMPARE(p.isAvailable(), available);
}

void TestQrzXml::configIsReadOnEveryCall()
{
    // A credential entered after start-up takes effect without re-wiring.
    QString password;
    QrzXmlLookupProvider p{QrzXmlConfig{
        .enabled  = [] { return true; },
        .username = [] { return QStringLiteral("N0CALL"); },
        .password = [&password] { return password; },
    }};
    QVERIFY(!p.isAvailable());
    password = QStringLiteral("secret");
    QVERIFY(p.isAvailable());
}

QTEST_GUILESS_MAIN(TestQrzXml)
#include "test_qrzxml.moc"
