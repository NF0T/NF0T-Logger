#pragma once

#include "SqlBackendBase.h"

/// SQLite database backend.
/// Config keys: "path" (absolute path to .db file)
class SqliteBackend : public SqlBackendBase
{
public:
    std::expected<void, QString> open(const QVariantMap &config) override;
    std::expected<void, QString> initSchema() override;
};
