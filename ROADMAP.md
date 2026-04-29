# NF0T Logger — Roadmap

This document captures planned features and enhancements. Items are grouped by milestone; specifics may shift as implementation reveals new constraints. Contributions toward any of these are welcome — see [CONTRIBUTING.md](CONTRIBUTING.md) before opening a PR.

---

## v0.3.0 — Revamped QSO Entry Panel ✓ shipped

The QSO entry panel was replaced with a two-column split layout sitting between the radio status bar and the main log table.

### Left column — entry form

Essential quick-log fields: UTC date/time (+ Now button), callsign, band/frequency (auto-filled from radio), mode/submode (auto-filled from radio), RST sent/received, and a single-line comment. A **Full Entry…** button and a **File → New QSO** menu action both open the full tabbed dialog.

### Right column — contact context panel

Populates automatically as a callsign is typed.

#### Callsign lookup

A `CallsignLookupProvider` interface supports pluggable data sources. The first provider shipped is the **QRZ XML Data API** (requires a QRZ subscription). Lookup fires on a 600 ms debounce timer after the callsign field is idle. Results are displayed in the context panel (name, location, license class, contact photo) and also pre-fill the relevant QSO fields (name, grid, country, state, county, continent, DXCC, CQ/ITU zone, lat/lon) using a two-layer priority model: station sources (WSJT-X, etc.) override lookup data; widget values always take priority over both. Clearing the callsign field resets the panel immediately.

#### Previous QSOs

Compact list of the last 5 contacts with the entered callsign, plus a real total count queried from the database.

### Full Entry dialog

Tabbed modal dialog with six sections — Contact, Activity (SOTA/POTA/WWFF/IOTA), Propagation, Contest, Satellite, and Notes — covering the full `Qso` field set. Pre-populates from the quick-entry panel when opened from the Full Entry button.

### Supporting infrastructure

`Callsign` utility class: validates ITU callsign format, parses prefix/base/suffix, extracts the base call for lookup, and resolves DXCC entity / CQ zone / ITU zone from a built-in prefix table.

---

## v0.4.0 — New Log / Log Rotation

Implements the **File → New Log** menu item, which currently shows a placeholder dialog.

The use case is primarily standalone SQLite deployments: a portable operation exports QSOs to ADIF (to be merged into the home station log later), then wants a clean slate for the next outing without carrying the previous session's records.

### Workflow

1. User chooses **File → New Log…**
2. A dialog offers two options:
   - **Archive and start fresh** — exports the current log to a user-chosen `.adi` file, then truncates the database
   - **Start fresh (discard current log)** — truncates the database after a confirmation prompt; no export
3. On confirmation the database is truncated, the table model is refreshed, and the window title updates to reflect the empty log

The schema (tables, migrations) is preserved; only QSO records are removed. This action is not available when the MariaDB backend is active, as that backend is intended for multi-instance shared deployments where log rotation is a DBA concern.

---

## vX.X.0 — Release Packaging and Distribution

Produce signed, self-contained installer/image artifacts and attach them to every GitHub release.

### Windows installer (NSIS via CPack)

1. Add `install()` rules and a CPack block to `CMakeLists.txt`
2. In the release CI job, run `windeployqt6 --dir staging/ NF0T-Logger.exe` to collect Qt DLLs, SQL driver, TLS backend, and image format plugins into a staging directory
3. Run `cmake --build --target package` with the CPack NSIS generator to produce `NF0T-Logger-<version>-win64.exe`

Note: this is a community GPG-signed installer, not a Microsoft Authenticode-signed one. Windows SmartScreen will warn on first run; Authenticode signing requires a paid EV certificate.

### Linux AppImage

1. Add a `.desktop` file and application icon to the repository
2. In the release CI job, use **linuxdeploy** with its Qt plugin to bundle the binary and all Qt libraries:
   ```
   linuxdeploy --appdir AppDir --executable NF0T-Logger --plugin qt --output appimage
   ```
3. Produces `NF0T-Logger-x86_64.AppImage` — no installation required

### GPG signing

Both artifacts (and any future macOS `.dmg`) are signed with a detached ASCII-armored signature:

```
gpg --detach-sign --armor NF0T-Logger-<version>-win64.exe
gpg --detach-sign --armor NF0T-Logger-x86_64.AppImage
```

The `.asc` files are uploaded alongside the binaries as release assets. The public key will be published to a keyserver and linked from the README. Users verify with `gpg --verify <file>.asc <file>`.

### CI release workflow

A new `.github/workflows/release.yml` triggers on `v*` tag pushes (separate from the existing build CI). It runs the Windows and Linux package jobs, imports the GPG key from a repository secret (`GPG_PRIVATE_KEY` + `GPG_PASSPHRASE`), signs both artifacts, and uploads them to the GitHub release via `gh release upload`.

---

## vX.X.0 — Database Migration Tool

Allow users to move their logbook between the SQLite and MariaDB backends without going through an intermediate ADIF export/import.

### Use case

The Settings page already lets users switch backends, but switching leaves the old database untouched and presents an empty log on the new backend. This feature adds a **Tools → Migrate Database…** action that copies all QSO records (and any future schema objects) from one backend to the other in a single guided workflow.

### Workflow

1. User opens **Tools → Migrate Database…**
2. A dialog shows the current (source) backend and asks the user to configure the target backend (path for SQLite, or host/port/credentials for MariaDB)
3. A dry-run pass reports how many QSOs would be transferred and flags any records that would be rejected (e.g. duplicate `callsign + datetime_on` on the target)
4. On confirmation, all QSOs are read from the source via `DatabaseInterface::fetchQsos()` and inserted into the target via `DatabaseInterface::insertQso()` in a single transaction where supported
5. On success, the dialog offers to switch the active backend in Settings to the target database

### Notes

- The active backend (`m_db`) is the source; the dialog opens a **second** backend instance for the target — both can coexist because each `SqlBackendBase` gets its own unique Qt SQL connection name (`NF0T_0`, `NF0T_1`, …). The second instance exists only for the duration of the migration; it is closed and destroyed before the dialog exits. The general design does not envision multiple databases open simultaneously.
- The dialog must collect target connection details from the user: a file-path chooser for SQLite, or host/port/user/password fields for MariaDB. MariaDB credentials are stored in the keychain via `SecureSettings`, so the dialog needs to handle the async load before it can pre-fill those fields.
- `initSchema()` on the target runs all pending migrations automatically before any records are written, so the target schema is always current.
- Duplicate handling should mirror ADIF import: unique-constraint violations are counted and reported rather than aborting the batch.
- MariaDB → SQLite is equally supported; the direction is determined solely by which backend is source vs. target.
- The transfer loop itself is backend-agnostic: `source->fetchQsos()` → `target->insertQso()` per record.

### Migration guard

While a migration is in progress the main window must be put into a locked state:

- WSJT-X and any other digital listener services are paused (or their `qsoLogged` signal is disconnected) so that incoming auto-log events do not write to the source database mid-transfer
- The QSO entry panel and **File → New QSO** action are disabled
- QSL upload and download actions are disabled
- The log table is set read-only (no edit/delete)

The locked state is lifted only after the target backend is closed and the dialog exits (whether the migration succeeded, failed, or was cancelled).

---

## vX.X.0 — CTY.dat Offline DXCC Lookup

Add a `CallsignLookupProvider` backed by the [CTY.dat country file](https://www.country-files.com/cty-dat-format/) (Jim Reisert AD1C / Big Cty). CTY.dat is the de-facto standard offline DXCC/zone/continent database used by CT, N1MM, and most contest loggers, and is updated several times per year as DXCC entities change.

### What CTY.dat provides

Each record in CTY.dat covers one DXCC entity and supplies:

- Country name, CQ zone, ITU zone, continent, latitude, longitude, UTC offset
- Primary DXCC prefix (prefixed with `*` for DARC WAEDC-only entities)
- Alias prefix list (comma-separated lines, terminated by `;`), with optional per-alias overrides:
  - `=CALL` — exact callsign match (not a prefix)
  - `(#)` — CQ zone override for that prefix
  - `[#]` — ITU zone override
  - `<lat/lon>` — lat/lon override
  - `{aa}` — continent override
  - `~#~` — UTC offset override

### Integration and provider precedence

CTY.dat is a local, file-backed source. The `CtyDatLookupProvider` will implement `CallsignLookupProvider` and emit `resultReady` synchronously (no network request). It fills the `CallsignLookupResult` fields that CTY.dat covers: `dxcc`, `cqZone`, `ituZone`, `cont`, `lat`, `lon`, and `country`. Fields that CTY.dat does not carry (name, QTH, grid, license class, image) remain empty.

The intended lookup chain, when both providers are enabled, is:

1. **CTY.dat** fires immediately on callsign entry and populates DXCC/zone/continent/country/lat/lon from the prefix table. This gives instant offline enrichment with no debounce delay.
2. **QRZ XML** fires after the existing 600 ms debounce and *supplements and updates* the CTY.dat result: it fills fields CTY.dat cannot provide (name, QTH, grid, license class, photo) and also overrides lat/lon and grid with the operator's actual registered location, which is more precise than CTY.dat's per-country centroid. QRZ zone data may also override CTY.dat where the two differ.
3. **Direct user entry** in the form always takes final precedence and overwrites any provider-supplied value.

This means the merge is not a simple fill-in-empty: QRZ is allowed to override a subset of fields (lat, lon, grid, and optionally zone) even when CTY.dat has already populated them. The exact set of "QRZ may override" fields should be defined explicitly in the merge logic.

`wireCallsignLookup()` currently wires a single provider. Supporting a chain will require either sequential wiring (CTY.dat result applied first, QRZ result merged on arrival) or a lightweight provider-aggregator class. The former is simpler and consistent with the existing architecture.

The CTY.dat provider supersedes the built-in prefix table in the `Callsign` utility class; that table can be retired once CTY.dat is the authoritative source.

### Prefix matching algorithm

1. Check alias list for an exact callsign match (`=CALL`) — these take priority
2. Strip portable suffixes and try progressively shorter prefixes until a match is found (longest-prefix-wins)
3. Apply any per-alias zone/continent/lat/lon overrides on top of the entity defaults
4. Parse top to bottom; the first matching entity wins on duplicate entries

### Bundled file and in-app updates

- A recent `cty.dat` is committed to the repository and shipped with every tagged release so the provider works out-of-the-box without any network access
- A **Settings → Callsign Lookup → Update CTY.dat** button fetches the latest file from `https://www.country-files.com/cty.dat`, validates it (checks that it parses without error and contains a reasonable number of entities), and installs it to the app data directory (`QStandardPaths::AppLocalDataLocation`). The installed file takes precedence over the bundled one; if the installed file is corrupt or absent the bundled file is used as fallback
- The settings page shows the currently active CTY.dat version (date line at the top of the file) and the date it was last updated

---

## Future / under consideration

These are ideas that have been discussed but not yet scoped:

- **Additional bulk actions** — e.g. bulk QSL upload, bulk re-export
- **Additional callsign lookup providers** — Hamcall, FCC ULS, national licensing databases
- **Log4OM-style contact tabs** — separate tabs for all contacts vs. recent contacts with a given station
