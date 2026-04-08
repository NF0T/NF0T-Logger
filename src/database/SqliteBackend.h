#pragma once

#include "SqlBackendBase.h"

/// SQLite database backend.
/// Config keys: "path" (absolute path to .db file)
class SqliteBackend : public SqlBackendBase
{
public:
    bool open(const QVariantMap &config) override;
    bool initSchema() override;
};
