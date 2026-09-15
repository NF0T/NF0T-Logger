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

## v0.4.0 — New Log / Log Rotation ✓ shipped

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

## v26.6.1 — Switch to Calendar Versioning ✓ shipped (CalVer)

Starting with the first release after v0.4.0, NF0T Logger will adopt **CalVer** versioning in the form `YY.MM.MICRO`, with an optional 4th segment for hotfixes:

| Tag | Meaning |
|---|---|
| `v26.6.1` | First release of June 2026 |
| `v26.6.2` | Second release of June 2026 |
| `v26.6.1.1` | Hotfix against the first June 2026 release |

### Changes required

- **`CMakeLists.txt`** — update `project(NF0T-Logger VERSION ...)` to the CalVer string at each release; CMake's 4-component integer version format is compatible with `YY.MM.MICRO.PATCH`
- **Release Drafter** — remove the `version-resolver` block from `.github/release-drafter.yml`; under CalVer the version is set explicitly at release time rather than derived from PR labels
- **ROADMAP.md** — future milestones will use CalVer identifiers rather than `vX.Y.Z` placeholders

Existing tags (`v0.1.0` through `v0.4.0`) remain unchanged in git history. The switch took effect with `v26.6.1`.

---

## v26.XX.N — Release Packaging and Distribution ✓ shipped

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

## v26.6.1 — Database Migration Tool ✓ shipped

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

## v26.6.1 — CTY.dat Offline DXCC Lookup ✓ shipped

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

## v26.9.2 — Off-UI-Thread I/O and Radio Backend Cleanup ✓ shipped

Moves the two remaining sources of UI-thread blocking — ADIF import and Hamlib CAT I/O — onto background threads, and fixes a data-integrity gap in custom SQLite deployments.

`v26.9.1` was tagged for this same content but never shipped a working release: it was published (creating the tag) before its build artifacts finished, which under GitHub's Immutable Releases feature permanently locked that release with zero binaries attached — and, it turns out, permanently burns the tag name itself even after the release object is deleted. `v26.9.1` does not exist as a usable release; `v26.9.2` is the real first artifact-bearing release of September 2026. See the CI fix below.

### ADIF import (#19)

Parsing and inserting records now runs on a background worker via `QtConcurrent::run()`, with its own dedicated database connection. Progress and cooperative cancellation are marshaled back to the UI thread through a `QFutureWatcher`. Inserts are batched into 500-record transactions to keep SQLite WAL write throughput up, and progress updates are throttled to every 25 records to avoid flooding the UI event queue.

### Hamlib CAT control (#18, #21, #22, #24)

Hamlib's blocking C API (`rig_open`, `rig_get_freq`, etc.) no longer runs on the UI thread. `HamlibBackend` was first moved wholesale onto its own `QThread` (#21), then split into a clean **worker-object** design (#22, #24): `HamlibWorker` owns the `RIG*` handle, the 500ms poll timer, and every blocking call on a dedicated thread, while `HamlibBackend` stays a normal `QObject` parented to `MainWindow` with the same synchronous-looking `RadioBackend` API (`connectRadio()`, `disconnectRadio()`, `setFreq()`, `setMode()`) every other backend uses. Shutdown still blocks the UI thread briefly if a Hamlib call is hung mid-teardown — an accepted tradeoff, since Hamlib's blocking API has no cancellation hook.

### Custom SQLite path and migration-lock UI coverage (#20, #23)

A user-configured custom SQLite path was silently ignored in favor of the default app-data location. Also closes UI-coverage gaps during a database migration (log table, filter bar, export, and settings actions weren't disabled the same way an ADIF import already disabled them) and a split-brain hazard where a background ADIF import could target a different database than the one currently open if Settings changed mid-session.

### Release pipeline fix (#27)

`.github/workflows/release.yml` triggered on a tag push, but the only way to create a tag through the GitHub Release UI/API is to publish the release first — and GitHub's Immutable Releases feature permanently locks a release's assets and tag the instant it publishes, no exceptions. "Publish, then upload" was therefore the *only* sequence the old design made possible, and it's exactly the sequence Immutable Releases forbids. Retargeted the trigger to the version bump landing on `main` instead: a `detect-version` job diffs `CMakeLists.txt`'s CalVer against existing tags so an ordinary push to `main` is a no-op, and the build job now attaches assets to the still-draft release and publishes only as its own last action — no more manual "push a tag" or "publish the draft" step for a human to get out of order.

---

## v26.XX.N — Quick Entry Panel Field Ordering

Field feedback from extended field use: the tab order and visual layout of `QsoQuickEntryPanel` need to match actual operating sequence.

### Default focus

Callsign already receives focus on `clearForm()`, `resetForm()`, and after `onLogClicked()` logs a contact ([QsoQuickEntryPanel.cpp](src/ui/entrypanel/QsoQuickEntryPanel.cpp)). This is the correct behavior and must be preserved — call it out explicitly here so it isn't lost incidentally while reordering the rest of the tab chain.

### New tab order

Callsign → RST Sent → RST Received → Comment → Band → Freq → Mode → Submode → Log button.

This changes two things from the current chain (`setTabOrder()` calls, currently `callsign → rstSent → rstRcvd → band → freq → mode → comment → logBtn`):

- **Comment moves up**, immediately after RST Received, instead of after Mode
- **Submode joins the tab chain** — it currently isn't wired into `setTabOrder()` at all despite being a populated field (`m_submode`, populated by `onModeChanged()`)

### Visual (left-to-right) order

Band, Freq, Mode, Submode — left to right, matching the suggested tab order for that trailing group. Row 2's current layout (`row2->addWidget(...)`) already places them in roughly this order; verify Submode is visually present in that row (it's currently constructed but not added to any layout) and lands after Mode.

---

## v26.XX.N — Live Clock Time Entry

Replaces the static "time + Now button" entry with a continuously running clock, so the operator doesn't need to remember to press Now before logging.

### Behavior

- `m_dateTime` ticks forward once per second (UTC) while unlocked, driven by a 1s `QTimer`, instead of only updating when the **Now** button is clicked
- A padlock-style toggle button replaces (or augments) the current **Now** button:
  - **Unlocked / live** — the clock keeps ticking; the field is read-only while live, since a live-updating value can't be hand-edited
  - **Locked** — clicking the padlock freezes the displayed time and makes the field editable, for manual entry (e.g. logging a contact after the fact)
  - The icon changes color/state between the two (e.g. open vs. closed padlock) so the current mode is visible at a glance
- Clicking the padlock again while locked resyncs to the current time and resumes ticking

### Recording time on log

`buildQso()` already reads `m_dateTime->dateTime()` at the moment `onLogClicked()` executes ([QsoQuickEntryPanel.cpp:607](src/ui/entrypanel/QsoQuickEntryPanel.cpp)), not at some earlier snapshot — this is the desired semantics and doesn't need to change. What needs to change is *what* `m_dateTime` holds at that moment: currently a static value last set by clearForm() or a Now click; going forward, the live-ticking current time whenever unlocked, or the manually-locked value otherwise.

### Notes

- `clearForm()` currently sets `m_dateTime` to `QDateTime::currentDateTimeUtc()` once (a static snapshot); it should instead reset the panel to the unlocked/live state so the clock resumes ticking for the next entry
- `onNowClicked()` / `m_nowBtn` are superseded by the lock toggle's unlock action, which has the same resync-to-now effect
- Files: [QsoQuickEntryPanel.h](src/ui/entrypanel/QsoQuickEntryPanel.h), [QsoQuickEntryPanel.cpp](src/ui/entrypanel/QsoQuickEntryPanel.cpp)

---

## Future / under consideration

These are ideas that have been discussed but not yet scoped:

- **Additional bulk actions** — e.g. bulk QSL upload, bulk re-export
- **Additional callsign lookup providers** — Hamcall, FCC ULS, national licensing databases
- **Log4OM-style contact tabs** — separate tabs for all contacts vs. recent contacts with a given station
