// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ryan Butler (NF0T)
#pragma once

#include "SqlBackendBase.h"

/// MariaDB / MySQL database backend.
/// Config keys: "host", "port" (int), "database", "username", "password"
class MariaDbBackend : public SqlBackendBase
{
public:
    std::expected<void, QString> open(const QVariantMap &config) override;
    std::expected<void, QString> initSchema() override;
};
