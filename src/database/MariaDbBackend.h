#pragma once

#include "SqlBackendBase.h"

/// MariaDB / MySQL database backend.
/// Config keys: "host", "port" (int), "database", "username", "password"
class MariaDbBackend : public SqlBackendBase
{
public:
    bool open(const QVariantMap &config) override;
    bool initSchema() override;
};
