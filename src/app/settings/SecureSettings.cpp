// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ryan Butler (NF0T)
#include "SecureSettings.h"

#include <qt6keychain/keychain.h>

const char *SecureSettings::SERVICE_NAME = "NF0T Logger";

const QStringList &SecureSettings::knownKeys()
{
    static const QStringList keys = {
        SecureKey::DB_MARIADB_PASSWORD,
        SecureKey::EQSL_PASSWORD,
        SecureKey::QRZ_API_KEY,
        SecureKey::CLUBLOG_PASSWORD,
        SecureKey::CLUBLOG_APP_KEY,
        SecureKey::LOTW_PASSWORD,
        SecureKey::QRZ_XML_PASSWORD,
    };
    return keys;
}

SecureSettings::SecureSettings() = default;

SecureSettings &SecureSettings::instance()
{
    static SecureSettings s;
    return s;
}

void SecureSettings::loadAll()
{
    if (m_loaded) return;

    const QStringList &keys = knownKeys();
    if (keys.isEmpty()) {
        m_loaded = true;
        emit loaded();
        return;
    }

    m_pendingJobs = keys.size();

    for (const QString &key : keys) {
        auto *job = new QKeychain::ReadPasswordJob(SERVICE_NAME, this);
        job->setAutoDelete(true);
        job->setKey(key);

        connect(job, &QKeychain::Job::finished, this, [this, job, key]() {
            if (job->error() == QKeychain::NoError) {
                m_cache[key] = job->textData();
            } else if (job->error() != QKeychain::EntryNotFound) {
                emit error(key, job->errorString());
            }
            if (--m_pendingJobs == 0) {
                m_loaded = true;
                emit loaded();
            }
        });

        job->start();
    }
}

QString SecureSettings::get(const QString &key) const
{
    return m_cache.value(key);
}

void SecureSettings::set(const QString &key, const QString &value)
{
    m_cache[key] = value;   // update cache immediately so reads are consistent

    auto *job = new QKeychain::WritePasswordJob(SERVICE_NAME, this);
    job->setAutoDelete(true);
    job->setKey(key);
    job->setTextData(value);

    connect(job, &QKeychain::Job::finished, this, [this, job, key]() {
        if (job->error() != QKeychain::NoError)
            emit error(key, job->errorString());
    });

    job->start();
}

void SecureSettings::remove(const QString &key)
{
    m_cache.remove(key);

    auto *job = new QKeychain::DeletePasswordJob(SERVICE_NAME, this);
    job->setAutoDelete(true);
    job->setKey(key);

    connect(job, &QKeychain::Job::finished, this, [this, job, key]() {
        if (job->error() != QKeychain::NoError && job->error() != QKeychain::EntryNotFound)
            emit error(key, job->errorString());
    });

    job->start();
}
