# NF0T Logger — Roadmap

This document captures planned features and enhancements. Items are grouped by milestone; specifics may shift as implementation reveals new constraints. Contributions toward any of these are welcome — see [CONTRIBUTING.md](CONTRIBUTING.md) before opening a PR.

---

## v0.2.0 — Multi-Select, Bulk Actions, and Log Filtering

### Multi-select in the main log table

The main QSO table will support standard multi-row selection:

- **Shift+click** — selects a contiguous range from the first selected row through the clicked row
- **Ctrl+click** — adds or removes individual rows from the selection

### Bulk action context menu

A right-click context menu on any selected row will offer actions that apply to all selected QSOs:

- **Delete selected** — removes all selected QSOs after confirmation
- **Export to ADIF** — writes all selected QSOs to a user-chosen `.adi` file

The context menu is designed to be extended with additional bulk actions in future milestones.

### Log filter bar

A compact filter bar above the log table wires up the existing `QsoFilter` infrastructure (already implemented at the database layer) into a live UI. All active controls compose with AND logic.

- **Callsign** — free-text substring match; updates the table as you type (debounced)
- **Band** — drop-down; "All bands" clears the filter
- **Mode** — drop-down; "All modes" clears the filter
- **Date range** — from/to date pickers (UTC); either bound may be left blank ("Any")
- **QSL status** — drop-down with unsent and pending-confirmation states for LoTW, eQSL, QRZ, and ClubLog
- **Clear** button — resets all controls and returns to the full log view

When a filter is active the status bar shows **"QSOs: X of Y"** so the total log size is always visible.

### "What's New" dialog

A modal dialog shown once on the first launch after an upgrade, displaying the release notes for the newly installed version.

- Release notes are fetched at launch from the **GitHub Releases API** for the installed version tag, so the release drafter output serves as the content automatically — no separate changelog file to maintain.
- The dialog is shown only once per version; a dismissed version is recorded in settings so it does not reappear.
- A **Help → What's New** menu item allows the user to re-open the dialog at any time.
- If the fetch fails (no network, GitHub unavailable), the dialog is silently skipped; it does not block startup.

---

## v0.3.0 — Revamped QSO Entry Panel

The current QSO entry panel will be replaced entirely with a two-column split layout that sits between the radio status bar and the main log table.

### Left column — entry form

The entry form is slimmed down to the fields needed for the vast majority of contacts. Fields not listed here move to the **Full Entry** dialog (see below).

**Essential fields retained in the quick-entry panel:**

- UTC date/time + Now button
- Callsign
- Band / Frequency (auto-filled from radio)
- Mode / Submode (auto-filled from radio)
- RST Sent / RST Received
- Comment (short free-text, single line)

**Removed from the panel (available via Full Entry):**

- Operator name
- Grid square (populated automatically via callsign lookup)
- TX power
- Activity / SIG / SIG_INFO (SOTA, POTA, WWFF, IOTA references)
- All propagation, contest, and satellite fields

A **"Full Entry…"** button at the bottom of the form opens the Full Entry dialog pre-populated with whatever has already been entered in the quick-entry panel. On save, the dialog writes the QSO directly to the log; the panel is then cleared as normal.

### Right column — contact context panel

A dynamic panel that populates as a callsign is entered in the entry form.

#### Callsign lookup

A generic `CallsignLookupProvider` interface will allow multiple data sources to be queried with a user-configurable priority order stored in Settings. The first provider to return a result wins.

**Planned providers (in priority order):**

1. **QRZ XML Data API** — requires a QRZ subscription (XML access); authenticates with QRZ username and password (separate from the existing QRZ logbook API key). Returns name, address, grid square, DXCC entity, license class, and more.
2. Further providers (Hamcall, FCC ULS, national licensing databases) are candidates for future milestones.

Lookup is triggered by a **debounce timer** — the lookup fires only after the callsign field has been idle for ~3 seconds, avoiding unnecessary API requests on every keystroke.

#### Previous QSOs

Shows a compact list of the last 5 contacts with the entered callsign:

```
Previous QSOs (12 total)
  2026-04-18   20m   FT8
  2026-01-03   40m   FT8
  2025-11-14   15m   SSB
  2025-09-02   20m   FT8
  2025-07-19   10m   FT8
```

Full contact history and main-table filtering based on entered callsign are candidates for a future milestone.

### Full Entry dialog

A modal dialog opened from the **"Full Entry…"** button in the quick-entry panel, or from a **File → New QSO** action. The dialog is tabbed to break the full `Qso` field set into logical sections:

- **Contact** — all quick-entry fields plus operator name, grid, TX power, comment
- **Activity** — SOTA, POTA, WWFF, IOTA references (SIG / SIG_INFO)
- **Propagation** — A-index, K-index, solar flux, band conditions
- **Contest** — contest name, serial number sent/received, exchange fields
- **Satellite** — satellite name, mode, orbital pass data
- **Notes** — free-text notes field (longer than the single-line comment)

The dialog pre-populates from the quick-entry panel when launched from the panel button. It can also be opened on an existing QSO (replacing the current inline edit workflow) for a future enhancement.

---

## Supporting infrastructure (v0.3.0)

### Callsign validation and parsing

A `Callsign` utility class that:

- Validates whether a string is a plausible amateur radio callsign
- Parses prefix, suffix, and portable designators (e.g. `W1AW/P`, `VE3XYZ/7`)
- Extracts the base callsign for lookup and duplicate checking
- Understands common prefix-to-DXCC mappings for prefill of country/zone fields

This class will be used by the entry form, the lookup trigger, and the duplicate check.

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

## Future / under consideration

These are ideas that have been discussed but not yet scoped:

- **Additional bulk actions** — e.g. bulk QSL upload, bulk re-export
- **Additional callsign lookup providers** — Hamcall, FCC ULS, national licensing databases
- **Log4OM-style contact tabs** — separate tabs for all contacts vs. recent contacts with a given station
