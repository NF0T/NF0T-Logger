# Contributing to NF0T-Logger

Thanks for your interest in contributing. NF0T-Logger welcomes bug reports, feature requests, and pull requests from anyone.

## Reporting bugs

Open a [GitHub Issue](https://github.com/NF0T/NF0T-Logger/issues) using the **Bug report** template. Include your OS, Qt version, radio backend, and steps to reproduce. Log output is especially helpful.

## Requesting features

Open an issue using the **Feature request** template. Describe the problem you want solved rather than a specific implementation — it helps start a better discussion.

## Branch workflow

The repository uses a two-branch model:

```
main ────●────────────────────────────●── tag v0.2.0
          \                          /
develop    ●──bugfix──feature──●──●──
                        \     /
                         feat
```

| Branch | Purpose |
|---|---|
| `main` | Always matches the latest release tag. Only receives merges from `develop` at release time, or direct bug fixes that are immediately merged forward to `develop`. |
| `develop` | Integration branch. All feature work targets here. Always contains everything in `main` plus whatever is planned for the next release. |

**For bug fixes:** fix on `main` first, then immediately merge `main` into `develop` so nothing diverges.

**For features:** branch off `develop`, merge back to `develop` when complete. `develop` is your integration surface — you can build it at any point to see the full picture of the next release.

**For releases:** merge `develop` into `main` with `--no-ff`, then tag.

## Submitting code

### Before you start

For anything beyond a small fix, open an issue first to discuss the approach. This avoids wasted effort if the direction doesn't fit the project.

### Setup

Follow the build instructions in [README.md](README.md) for your platform. The project uses CMake; `compile_commands.json` is generated automatically for IDE/tooling support.

Developer conventions are documented in [CLAUDE.md](CLAUDE.md). The short version:

- C++23, no compiler extensions (`CMAKE_CXX_EXTENSIONS OFF`)
- New-style Qt signal/slot connections (`&Class::signal`)
- `QStringLiteral` for Qt string literals
- No comments that explain *what* — only *why* when non-obvious
- Database operations return `std::expected<T, QString>`, not exceptions
- All persistent settings go through `Settings::instance()` or `SecureSettings::instance()`

### Pull requests

1. Fork the repository and create a branch off `develop`
2. Make your changes — one logical change per PR
3. Verify it builds cleanly on your platform (CI will check Linux, Windows, and macOS automatically)
4. Open a PR against `develop` with a clear description of what changed and why

### New backends

The architecture is designed for extension without touching `MainWindow`:

- **Radio backend** — subclass `RadioBackend`, call `wireRadioBackend()` in `MainWindow`
- **Digital listener** — subclass `DigitalListenerService`, call `wireDigitalListener()`
- **QSL service** — subclass `QslService`
- **Database backend** — implement `DatabaseInterface`, add a factory case in `MainWindow`

### Database schema changes

Add a new migration entry in `SqlBackendBase`. Never alter existing migrations — they may already have run on users' databases.

## AI-assisted development

This project is developed with AI assistance — specifically [Claude](https://claude.ai) by Anthropic — and contributions produced with AI tools are welcome. The `Co-Authored-By: Claude` trailers you will see in the commit history reflect this transparently.

The expectation is the same regardless of how code was produced: you should understand what you are submitting and be able to discuss it in review. The quality bar does not change based on the authoring tool.

## License

By contributing you agree that your contributions will be licensed under the **GNU General Public License v3.0 or later**, the same license as the project. You retain copyright on your contributions.
