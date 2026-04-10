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
