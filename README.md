# NF0T-Logger

A cross-platform amateur radio contact logger for Linux, Windows, and macOS, built with C++23 and Qt 6.

NF0T-Logger grew out of wanting a logger that worked reliably on Linux, felt lighter than Log4OM, and was open enough for others to extend. It is designed to be a daily-driver logger for the NF0T station and is open to use and contribution by any amateur radio operator.

---

## Features

- **Logbook management** — create, edit, search, and filter QSO records with a comprehensive field set (signal reports, propagation data, SOTA/POTA/WWFF/IOTA references, satellite, contest fields, and more)
- **ADIF v3 import/export** — spec-compliant length-based tokeniser; compatible with Log4OM, WSJT-X, and other logging software
- **CAT radio control** — Hamlib (serial and network) and TCI (ExpertSDR / AetherSDR) backends; frequency and mode sync with the entry panel
- **WSJT-X integration** — UDP listener (multicast and unicast) with auto-logging; coexists with GridTracker2 and JTAlert on the same port
- **QSL confirmation management** — upload and download for LoTW, eQSL, QRZ, and ClubLog; confirmation dates written back to the local log
- **Secure credential storage** — passwords and API keys stored in the system keychain (Windows Credential Manager, macOS Keychain, KWallet / libsecret on Linux)
- **Flexible database backends** — SQLite (default, zero-config) and MariaDB
- **Log4OM v2 migration tool** — standalone CLI to migrate an existing Log4OM v2 MariaDB database

---

## Screenshots

*Screenshots coming soon.*

---

## Requirements

| Dependency | Version | Notes |
|---|---|---|
| Qt 6 | ≥ 6.5 | Core, Widgets, Network, Sql, WebSockets |
| CMake | ≥ 3.20 | |
| C++ compiler | GCC 13 / Clang 16 / MSVC 2022 | C++23 required |
| QtKeychain | any recent | system keychain integration |
| Hamlib | any recent | optional — CAT control |
| MariaDB Connector/C | any | optional — only if using MariaDB backend |

---

## Building

### Linux

```bash
# Install Qt 6, QtKeychain, and optionally Hamlib via your package manager, e.g.:
# Arch:   sudo pacman -S qt6-base qt6-websockets qtkeychain hamlib
# Ubuntu: sudo apt install qt6-base-dev libqt6websockets6-dev qtkeychain-qt6-dev libhamlib-dev

cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/NF0T-Logger
```

### Windows

The recommended toolchain is Qt's bundled MinGW. QtKeychain must be built from source once:

```bash
# With Qt's CMake and Ninja in your PATH:
git clone https://github.com/frankosterfeld/qtkeychain.git
cmake -S qtkeychain -B qtkeychain-build -G Ninja \
  -DCMAKE_PREFIX_PATH="C:/Qt/6.x.x/mingw_64" \
  -DCMAKE_INSTALL_PREFIX="C:/Qt/qtkeychain" \
  -DBUILD_WITH_QT6=ON -DBUILD_TRANSLATIONS=OFF -DBUILD_TESTING=OFF
cmake --build qtkeychain-build
cmake --install qtkeychain-build

# Then build the logger:
cmake -B build-win -G Ninja \
  "-DCMAKE_PREFIX_PATH=C:/Qt/6.x.x/mingw_64;C:/Qt/qtkeychain" \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build-win --parallel
windeployqt6 build-win/NF0T-Logger.exe
```

### macOS

```bash
brew install qt qtkeychain hamlib
cmake -B build-mac -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="$(brew --prefix qt);$(brew --prefix qtkeychain)"
cmake --build build-mac --parallel
```

---

## First run

On first launch NF0T-Logger creates a SQLite database at:

- **Linux:** `~/.local/share/NF0T/NF0T Logger/logbook.sqlite`
- **Windows:** `%LOCALAPPDATA%\NF0T\NF0T Logger\logbook.sqlite`
- **macOS:** `~/Library/Application Support/NF0T/NF0T Logger/logbook.sqlite`

Open **Edit → Settings** to configure your station details, radio backend, WSJT-X UDP listener, and QSL service credentials before your first QSO.

---

## Migrating from Log4OM v2

A standalone migration tool is included:

```bash
./build/log4om-migrate --host 127.0.0.1 --user root --db log4om \
    --station YOURCALL
```

Run with `--help` for all options including dry-run mode.

---

## Project status

NF0T-Logger is under active development and currently at **v0.1.0**. The core logging, radio control, and QSL workflows are functional. Rough edges exist — contributions and bug reports are welcome.

---

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md). Bug reports and feature requests go in [GitHub Issues](https://github.com/NF0T/NF0T-Logger/issues).

---

## License

NF0T-Logger is free software released under the **GNU General Public License v3.0 or later**.
Copyright (C) 2026 Ryan Butler (NF0T).

See [LICENSE](LICENSE) for the full text.
