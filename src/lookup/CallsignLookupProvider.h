// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ryan Butler (NF0T)
#pragma once

#include <QObject>
#include <QString>

#include "CallsignLookupResult.h"

/// Abstract base for callsign lookup providers (QRZ XML, Hamcall, FCC ULS, …).
///
/// Concrete providers implement lookup(), which is asynchronous — it returns
/// immediately and emits resultReady() or lookupFailed() when the query
/// completes. MainWindow wires all providers generically through this interface.
///
/// Adding a new provider: subclass this, implement the three pure virtuals,
/// and register an instance with MainWindow (step 6+).
class CallsignLookupProvider : public QObject
{
    Q_OBJECT

public:
    explicit CallsignLookupProvider(QObject *parent = nullptr) : QObject(parent) {}
    ~CallsignLookupProvider() override = default;

    /// Short human-readable name shown in Settings (e.g. "QRZ XML").
    virtual QString displayName() const = 0;

    /// Returns true if the provider is configured and ready to accept queries.
    /// Typically checks that credentials are present in SecureSettings.
    virtual bool isAvailable() const = 0;

    /// Begin an asynchronous lookup for callsign.
    /// Emits resultReady() on success or lookupFailed() on error.
    /// Calling lookup() while a previous query for the same callsign is in
    /// flight is safe — providers should coalesce or ignore duplicate requests.
    virtual void lookup(const QString &callsign) = 0;

signals:
    void resultReady(const QString &callsign, const CallsignLookupResult &result);
    void lookupFailed(const QString &callsign, const QString &error);
};
