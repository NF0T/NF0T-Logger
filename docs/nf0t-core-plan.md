# nf0t-core — Extraction and Re-integration Plan

Status: draft, 2026-11-15. Tracks the roadmap item [nf0t-core Shared Library](../ROADMAP.md#v26xxn--nf0t-core-shared-library).

## Goal

Move NF0T Logger's application-independent code into a static library, `nf0t-core`, that lives in its own repository and is consumed as a git submodule by NF0T Logger and by the planned CC Companion (3905 Century Club logger).

The extraction must be **behaviour-neutral for NF0T Logger**: no user-visible change, no schema change, no settings change.

## Strategy: carve in place, then split

The work happens in two phases so that every step is an ordinary, reviewable PR to `develop`:

- **Phase A — carve in place.** Build `nf0t-core` as a library target inside this repo at `libs/nf0t-core/`. Its CMake can only see its own sources, so **the compiler proves the library has no hidden dependency on the app**. Ship it in a normal release.
- **Phase B — split.** Extract `libs/nf0t-core/` with its history into `NF0T/nf0t-core`, then replace the directory with a submodule **at the same path**. The app's build does not change.

Doing the split last means a mistake in Phase A is fixed with a normal PR, not a cross-repo dance.

## What moves, what stays

Verified against `src/` at `7f62ba3` by tracing `#include`s and `Settings::instance()` / `SecureSettings` usage.

| Component | Files | Couplings today | Decision |
|---|---|---|---|
| QSO value type, filter | `core/logbook/Qso.h`, `core/logbook/QsoFilter.h` | none | Move |
| ADIF | `core/adif/AdifParser.*`, `core/adif/AdifWriter.*` | `Qso.h` only | Move |
| Callsign, Maidenhead, band plan | `core/Callsign.*`, `core/Maidenhead.*`, `core/BandPlan.*` | none | Move |
| Database | `database/DatabaseInterface.h`, `SqlBackendBase.*`, `SqliteBackend.*`, `MariaDbBackend.*` | `Qso.h`, `QsoFilter.h` only | Move (MariaDB is a runtime Qt SQL driver, no build dependency) |
| Lookup interface | `lookup/CallsignLookupProvider.h`, `lookup/CallsignLookupResult.h` | none (a doc comment mentions SecureSettings) | Move |
| CTY.dat provider | `lookup/CtyDatLookupProvider.*` | `Settings::ctyDatEnabled()`, `ctyDatCustomPath()`, `:/data/cty.dat` app resource | Move after config injection (A2) |
| QRZ XML provider | `lookup/QrzXmlLookupProvider.*` | `Settings::callsignLookupEnabled()`, `qrzXmlUsername()`, `qrzXmlPassword()` (keychain) | Move after config injection (A2) |
| Table model | `core/logbook/QsoTableModel.*` | `Settings::useMetricUnits()`; it is presentation | **Stays**, relocated to `src/ui/` |
| QSL services, radio, WSJT-X, settings, UI | `qsl/`, `radio/`, `wsjtx/`, `app/`, `ui/` | app-specific | Stay |

Qt modules the library needs: `Core`, `Sql`, `Network` (QRZ XML). No `Widgets`, no `qt6keychain`.

## Prerequisite

The test infrastructure (`tests/`, `enable_testing()` / `add_subdirectory(tests)`, and a `ctest` step in all three CI jobs) arrives with the TCI 2.1 work in PR #34. That PR should land first, so A1 adds to existing infrastructure rather than creating it.

## Phase A — carve in place (PRs to `develop`)

### A1. Characterization tests (no production changes)

Pin current behaviour before anything moves. Qt Test, compiling the `src/` files directly as `test_tci` does today.

- [ ] `Callsign`: validation, prefix/base/suffix parsing, portable designators, DXCC/zone resolution for a table of known calls
- [ ] `Maidenhead`: grid ↔ lat/lon for 4- and 6-character grids; haversine distance against known pairs
- [ ] `BandPlan`: frequency → band at band edges and outside any band
- [ ] ADIF: parse → write → parse round trip is lossless across every populated `Qso` field, including the `APP_NF0T_*` fields; length-based tokenising with multibyte UTF-8 values
- [ ] `SqliteBackend`: `initSchema()` on an empty temp file runs every migration; insert → fetch round trip; duplicate `callsign + datetime_on` is rejected, not thrown
- [ ] `CtyDatLookupProvider`: a small fixture `cty.dat` covering `=CALL` exact matches, longest-prefix-wins and zone overrides

Gate: tests pass on Linux and Windows CI.

### A2. Decouple the lookup providers and the table model (behaviour-neutral)

- [ ] Move `core/logbook/QsoTableModel.*` to `src/ui/QsoTableModel.*`; update the two includes (`MainWindow.cpp`, `QslColumns.cpp`).
- [ ] Give the providers **pull-based** configuration through injected callables, so values are still read at call time exactly as `Settings::instance()` is read today, and a credential change takes effect without re-wiring:

  ```cpp
  // lookup/CtyDatLookupProvider.h
  struct CtyDatConfig {
      std::function<bool()>    enabled;
      std::function<QString()> customPath;   // empty = none
      QString                  bundledPath;  // e.g. ":/data/cty.dat", owned by the app
  };
  explicit CtyDatLookupProvider(CtyDatConfig config, QObject *parent = nullptr);

  // lookup/QrzXmlLookupProvider.h
  struct QrzXmlConfig {
      std::function<bool()>    enabled;
      std::function<QString()> username;
      std::function<QString()> password;
  };
  explicit QrzXmlLookupProvider(QrzXmlConfig config, QObject *parent = nullptr);
  ```

- [ ] `MainWindow::wireCallsignLookup()` builds those configs from lambdas over `Settings::instance()`. This is the only call site.
- [ ] Remove `#include "app/settings/Settings.h"` from everything under `core/`, `database/`, `lookup/`.

Gate: A1 tests still pass; manual check that QRZ lookup and CTY.dat enrichment behave as before (including the Settings → Update CTY.dat path).

### A3. Create the library target

- [ ] `git mv` the "Move" rows above into `libs/nf0t-core/src/`, **keeping their relative paths** (`core/…`, `database/…`, `lookup/…`). Every existing `#include "core/…"` stays valid, so there is no include churn in the app.
- [ ] `libs/nf0t-core/CMakeLists.txt`:
  - `add_library(nf0t-core STATIC …)`, `target_include_directories(nf0t-core PUBLIC src)`
  - `target_link_libraries(nf0t-core PUBLIC Qt6::Core Qt6::Sql Qt6::Network)`
  - C++23, `CXX_EXTENSIONS OFF`, `AUTOMOC ON` set on the target, because a top-level `qt_standard_project_setup()` won't exist once it's a separate repo
  - Buildable standalone: `find_package(Qt6 …)` itself when it is the top-level project (`CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR`; `PROJECT_IS_TOP_LEVEL` needs CMake 3.21 and the repo's floor is 3.20)
  - **No include path to `../../src`.** This is the enforcement mechanism: any remaining app coupling is a compile error.
- [ ] Move the A1 tests to `libs/nf0t-core/tests/` so they travel with the library in Phase B.
- [ ] `src/CMakeLists.txt`: drop the moved files from `SOURCES`/`HEADERS`; `target_link_libraries(NF0T-Logger PRIVATE nf0t-core …)`.
- [ ] Top-level `CMakeLists.txt`: `add_subdirectory(libs/nf0t-core)` before `src`.
- [ ] `tools/log4om-migrate`: stop compiling `SqlBackendBase.cpp`/`SqliteBackend.cpp` directly; link `nf0t-core`.
- [ ] CI (`ci.yml`): add a `ctest --output-on-failure` step to each build job.
- [ ] Update the "Source layout" section of `CLAUDE.md` and the build notes in `README.md`/`CONTRIBUTING.md`.

Gate: Linux, Windows and macOS CI green; the app binary behaves identically. Smoke list: ADIF import and export, callsign lookup (QRZ + CTY.dat), New Log, Migrate Database (SQLite ↔ MariaDB), log4om-migrate against a test DB.

### A4. Ship it

Release normally from `develop` → `main`. Phase B starts only after a release containing A3 has been used for a while without regressions.

## Phase B — split into `NF0T/nf0t-core`

### B1. Create the library repository with history

- [ ] Extract with `git filter-repo`, including the **pre-move** paths so `git log --follow` survives the A3 move:
  `--path libs/nf0t-core/ --path src/core/adif/ --path src/core/Callsign.cpp … --path-rename libs/nf0t-core/:`
  (list every file from the "Move" rows; leave out `QsoTableModel`).
- [ ] Add `LICENSE` (GPL-3.0, same as the app), `README.md` (purpose, build, consumers), `CLAUDE.md` (conventions copied from the app: `std::expected` DB results, `QStringLiteral`, no `M_PI`, migration rules).
- [ ] CI: build and test on Linux, Windows and macOS.
- [ ] Tag the first version with CalVer to match the app (e.g. `v26.12.1`).

### B2. Re-integrate as a submodule

- [ ] In NF0T Logger: `git rm -r libs/nf0t-core` and `git submodule add https://github.com/NF0T/nf0t-core libs/nf0t-core`, pinned to the tag. Same path, so the CMake from A3 is unchanged.
- [ ] Every `actions/checkout` step that builds gets `submodules: recursive`: the three jobs in `ci.yml` and the two build jobs in `release.yml`. `detect-version` and `release-drafter.yml` don't build and don't need it.
- [ ] Docs: `git clone --recursive`, and `git submodule update --init` for existing clones.
- [ ] Release notes caveat: GitHub's auto-generated "Source code" archives **do not include submodules**. Installers and AppImages are unaffected because the library is linked statically.

Gate: a fresh `git clone --recursive` builds and passes CI on all three platforms.

## Working with the split afterwards

- A core change is a PR to `nf0t-core` → tag → a PR to each app that bumps the submodule. Never point a submodule at an untagged commit on `main`.
- **Schema migrations move with the database code.** The existing rule still applies inside `nf0t-core`: every migration goes into both the SQLite and MariaDB paths in the same commit.
- `APP_NF0T_*` ADIF fields stay as they are: the library is NF0T-branded, and CC Companion reads and writes them the same way.

## Open questions

- [ ] **Schema extension point.** `SqlBackendBase` owns NF0T Logger's QSO schema. CC Companion needs its own tables (claims, applications, QSL images) on top. Decide before B1 whether `SqlBackendBase` gets an API for app-registered migrations under a separate version key, or whether the companion keeps its tables behind its own migration runner on the same connection. Either way, NF0T Logger's schema must not change.
- [ ] **Bundled cty.dat.** It stays an app resource in this plan, and the provider receives `bundledPath`. Alternative: the library owns `data/cty.dat` via `qt_add_resources` on the static target, so both apps share one copy. Revisit once CC Companion exists.
- [ ] **Namespace.** No namespace change in this plan, to avoid churn. Whether to add one (`nf0t::`) is a later, separate decision.
