# CLAUDE.md — NF0T-Logger

Developer guide for Claude Code and human contributors.

## Project overview

NF0T-Logger is a cross-platform amateur radio contact (QSO) logger written in C++23 with Qt 6. It replaces Log4OM v2 for the NF0T station. Core capabilities: logbook management with ADIF v3 import/export, CAT radio control (Hamlib and TCI), WSJT-X digital mode integration, and QSL confirmation management (LoTW, eQSL, QRZ, ClubLog).

## Build

### Dependencies
- CMake ≥ 3.20
- C++23 compiler (GCC 13+, Clang 16+, MSVC 2022+)
- Qt 6 — Core, Widgets, Network, Sql, WebSockets
- [QtKeychain](https://github.com/frankosterfeld/qtkeychain) — system keychain integration
- Hamlib (optional) — CAT radio control; detected via pkg-config

### Linux
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

### Windows (Qt-bundled MinGW toolchain)
```bash
cmake -B build-win -G Ninja \
  "-DCMAKE_PREFIX_PATH=C:/Qt/6.x.x/mingw_64;C:/Qt/qtkeychain" \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build-win --parallel
# Deploy Qt runtime alongside the exe
windeployqt6 build-win/NF0T-Logger.exe
```

QtKeychain must be built from source on Windows and installed to a prefix included in `CMAKE_PREFIX_PATH`. See the Windows build notes in the repository for details.

### macOS
```bash
cmake -B build-mac -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH=$(brew --prefix qt6):$(brew --prefix qtkeychain)
cmake --build build-mac --parallel
```

## Source layout

```
src/
  main.cpp
  app/
    MainWindow.h/.cpp          # Central window; wires all backends via wireRadioBackend()
                               # and wireDigitalListener()
    settings/
      Settings.h/.cpp          # Typed singleton; all persistent (non-secret) config
      SecureSettings.h/.cpp    # Async keychain loader; call loadAll() at startup
      SettingsDialog.h/.cpp    # Multi-page settings UI
      pages/                   # One .h/.cpp per settings page
  core/
    logbook/
      Qso.h                    # Central data struct (~160 fields, plain value type)
      QsoTableModel.h/.cpp     # Qt model for the main log table
      QsoFilter.h/.cpp         # Query/filter helpers
    adif/
      AdifParser.h/.cpp        # ADIF v3 length-based tokeniser (spec-compliant)
      AdifWriter.h/.cpp
    BandPlan.h/.cpp
    Maidenhead.h/.cpp          # Grid square ↔ lat/lon + haversine distance
  database/
    DatabaseInterface.h        # Pure abstract interface
    SqlBackendBase.h/.cpp      # Shared SQL + schema migration logic
    SqliteBackend.h/.cpp
    MariaDbBackend.h/.cpp
  radio/
    RadioBackend.h             # Abstract base: freqChanged, modeChanged, transmitChanged
    HamlibBackend.h/.cpp       # Hamlib CAT (compile-time optional via HAVE_HAMLIB)
    TciBackend.h/.cpp          # ExpertSDR / AetherSDR TCI over WebSocket
    DigitalListenerService.h   # Abstract base for digital mode listeners
  wsjtx/
    WsjtxService.h/.cpp        # WSJT-X UDP binary protocol (schema v2)
  qsl/
    QslService.h               # Abstract base
    LoTwService.h/.cpp
    EqslService.h/.cpp
    QrzService.h/.cpp
    ClubLogService.h/.cpp
  ui/
    RadioPanel.h/.cpp          # RX/TX pill indicators + frequency display
    entrypanel/
      QsoEntryPanel.h/.cpp     # New QSO entry form
    QsoEditDialog.h/.cpp
    QslColumns.h/.cpp
    QslUploadDialog.h/.cpp
    QslDownloadDialog.h/.cpp

tools/
  log4om-migrate/              # Standalone CLI: migrates QSOs from Log4OM v2 MariaDB
```

## Architecture rules

**Adding a new radio backend:** Subclass `RadioBackend`, implement the pure virtuals, emit `freqChanged` / `modeChanged` / `transmitChanged` / `connected` / `disconnected` / `error`. Call `wireRadioBackend()` in `MainWindow` — no other MainWindow changes needed.

**Adding a new digital listener:** Subclass `DigitalListenerService`, call `wireDigitalListener()` in `MainWindow`.

**Adding a new QSL service:** Subclass `QslService`.

**Database operations** return `std::expected<T, QString>`. Never throw from DB code.

**Settings access** always goes through `Settings::instance()` (non-secret) or `SecureSettings::instance()` (passwords, API keys). Do not read `QSettings` directly anywhere else.

## Code conventions

- C++23, `CMAKE_CXX_EXTENSIONS OFF` — no GNU/MSVC extensions
- No `M_PI`; use `std::numbers::pi`
- Qt signal/slot syntax: always new-style (`&Class::signal`)
- No comments explaining *what* code does; only *why* when non-obvious
- `QStringLiteral` for all string literals used with Qt APIs
- Database schema changes: add a new migration in `SqlBackendBase` — never alter existing migrations

## Known issues / tabled work

- **TCI TX indicator** — AetherSDR does not emit the `TRX:{trx},{state}` command on PTT. Logger code is correct; waiting on AetherSDR side.
- **WSJT-X multicast** — interface selector added to settings. If WSJT-X is bound to loopback, check the loopback interface in Settings → WSJT-X.
- **Serial port default** — Settings defaults to `/dev/ttyUSB0` on all platforms. On Windows use `COM1` etc.; on macOS `/dev/cu.usbserial-*`.
- **tQSL path placeholder** — Settings page shows `/usr/bin/tqsl`; adjust for Windows/macOS.
