---
title: Chytanka Releasing
nav_exclude: true
---

# Releasing «Читанка» (Chytanka)

Fork-only doc. Covers how Chytanka versions and tags a release, the one-time
GitHub Actions setting every new fork needs, and what to re-check after
rebasing onto a new CrossInk tag. Not published in CrossInk's own docs nav.

## Version scheme

The `ua` PlatformIO env (`platformio.chytanka.ini`) builds `CROSSINK_VERSION`
as:

```
${crossink.version}.${chytanka.revision}-ua
```

- `crossink.version` is CrossInk's own `[crossink] version` from
  `platformio.ini` (e.g. `1.6.0`) — do not edit it here.
- `chytanka.revision` is Chytanka's own release counter, declared in
  `[chytanka] revision` at the top of `platformio.chytanka.ini`. It starts at
  `0` for a given CrossInk base and increments by one on every Chytanka-only
  release built from that base.
- After rebasing onto a new CrossInk version (`[crossink] version` bumps),
  reset `revision` back to `0` — the new CrossInk base restarts the counter.

Example: two Chytanka releases on top of CrossInk `1.6.0` are
`1.6.0.0-ua` and `1.6.0.1-ua`. After rebasing onto CrossInk `1.6.1`, the next
release is `1.6.1.0-ua`.

### Why this shape

The OTA client's version comparator (`src/network/OtaUpdater.cpp`, functions
`parseVersion`/`compareVersions`, roughly lines 61–113) reads **up to 4**
numeric, dot-separated segments starting at an optional leading `v`/`V`, and
stops at the first character that is neither a digit nor `.` — so the `-ua`
suffix is never parsed as a version segment and never affects the comparison.
Missing trailing segments default to `0`.

Traced by hand (host-testable coverage isn't practical here: the parser lives
in the non-`SIMULATOR` half of `OtaUpdater.cpp`, which pulls in
`Arduino.h`/`esp_http_client.h`/`mbedtls` and doesn't compile for the native
test target; the existing `test/ota_staging_guard` test for this same file
works around that by pattern-matching the source text instead of calling into
it, which wouldn't exercise the numeric comparison logic either):

| Compare (latest vs current) | Parsed segments | Result |
| --- | --- | --- |
| `1.6.0.1-ua` vs `1.6.0.0-ua` | `[1,6,0,1]` vs `[1,6,0,0]` | latest is newer (segment 3: `1 > 0`) |
| `1.6.1.0-ua` vs `1.6.0.5-ua` | `[1,6,1,0]` vs `[1,6,0,5]` | latest is newer (segment 2: `1 > 0`, decided before segment 3 is even looked at) |
| `1.6.0.1-ua` vs `1.6.0-ua` | `[1,6,0,1]` vs `[1,6,0,0]` | latest is newer (segment 3: `1 > 0`; `1.6.0-ua` has no 4th segment, defaults to `0`) |

**Caveat found while tracing:** `1.6.0.0-ua` and `1.6.0-ua` parse to the
*identical* segment array `[1,6,0,0]` — the comparator treats them as equal,
not `1.6.0.0-ua > 1.6.0-ua`. This only matters for the one pre-this-change
release that used the bare `${crossink.version}-ua` scheme (no revision
digit); a device already on that build will not see `1.6.0.0-ua` as an OTA
update. It self-resolves at the next release (`1.6.0.1-ua`, or any later
revision) since that does compare strictly greater than both. No code change
needed — noted here so nobody "fixes" the comparator into a rebase conflict
with upstream CrossInk.

## Tag rules

- Tag format: `v<crossink.version>.<chytanka.revision>`, e.g. `v1.6.0.1` for
  `CROSSINK_VERSION=1.6.0.1-ua`. (The tag itself has no `-ua` suffix; that
  suffix only appears inside the compiled version string.)
- A tag must start with a digit, or `v` followed by a digit — required by
  `startsWithNumberAfterOptionalV()` in the OTA parser above.
- **Never** publish a Chytanka release as a GitHub pre-release or draft. The
  OTA client only looks at `.../releases/latest`, which GitHub only ever
  populates from the most recent non-prerelease, non-draft release. A
  pre-release/draft is simply invisible to devices, not a safe way to stage
  one.
- Release asset: `firmware-x3-x4.bin`, built with `pio run -e ua`. This name
  must match what `OtaUpdater.cpp`'s `isMatchingFirmwareAssetName()` expects
  for `CROSSINK_FIRMWARE_DEVICE_TYPE="x3-x4"` (exact name, or
  `firmware-x3-x4-<anything>.bin`).

## Default device name (`CHYTANKA`)

`CrossPointSettings::getDefaultDeviceName()` (`src/CrossPointSettings.cpp`,
~line 245) returns `"Chytanka X3 CrossInk"` / `"...X4"` under `CHYTANKA`
for a fresh device (no saved `deviceName`); a saved name always wins
(`getEffectiveDeviceName()`). Both strings are exactly 20 bytes, equal to
`CrossPointSettings::MAX_DEVICE_NAME_LENGTH` (20, from the fixed
`deviceName[21]` settings buffer), so they fit verbatim with no truncation
anywhere:

- **Nearby book transfer / stats sync / position sync**
  (`NearbyBookTransferActivity.cpp`, `NearbyStatsSyncActivity.cpp`,
  `NearbyBookPositionSyncActivity.cpp`): all bound the outgoing copy with
  `std::min(strlen(name), MAX_DEVICE_NAME_LENGTH)` before `memcpy`. At exactly
  20 bytes the default is copied in full — no truncation at the peer.
- **KOReader sync** (`KOReaderSyncActivity.cpp`): `progress.device` is a
  `std::string` sent as a JSON field — no truncation, full name round-trips.
  Local display of a peer's device name renders through `snprintf` into a
  64-byte stack buffer, comfortably fits.
- **Settings → device name editor** (`SettingsActivity.cpp`
  `openStringEditor()`): the editor's `maxLength` is
  `MAX_DEVICE_NAME_LENGTH` (20), and the *initial text* — the unedited
  default — is now exactly 20 bytes, so it no longer arrives over the
  editor's own limit. Confirming without editing saves cleanly.
- **Web UI, hostname/mDNS, AP SSID, User-Agent, BLE**: none of these read
  `getEffectiveDeviceName()`/`getDefaultDeviceName()` at all. Wi-Fi
  hostname/mDNS/AP name are the fixed strings `"crosspoint"` /
  `"CrossPoint-Reader-<mac>"` (`CrossPointWebServerActivity.cpp`,
  `CalibreConnectActivity.cpp`, `WifiSelectionActivity.cpp`); OTA/download
  User-Agent uses `CROSSINK_VERSION`, not the device name; there is no
  Bluetooth/BLE code in this repo.

If the default ever grows again, re-check the same consumers: any name over
`MAX_DEVICE_NAME_LENGTH` (20) still degrades safely at the Nearby/transfer
paths (silent truncation via `std::min`, not a memory-safety issue) but will
reintroduce the Settings-editor pre-fill rough edge described above.

## One-time fork setup: disable CrossInk's release workflows

Right after creating the `chytanka` fork on GitHub, go to **Settings →
Actions → General** (or disable the individual workflows under the Actions
tab) and disable `release.yml` ("Compile Release") and
`release_candidate.yml` ("Compile Release Candidate").

Both ship from upstream CrossInk as `workflow_dispatch`-only (not triggered
automatically by a tag push), but a fork inherits them enabled by default,
and either one can still be run manually from the Actions tab — by anyone
with write access, or by an automated workflow trigger nobody remembers
setting up. If either ever runs in the fork, it publishes a *stock CrossInk*
build as the repo's "latest" GitHub Release. Because Chytanka's OTA URL
(`CROSSINK_OTA_RELEASE_URL`, see below) points at *this* fork's
`/releases/latest`, every Chytanka device in the field would then offer, and
on tap install, stock CrossInk — losing the Chytanka brand screens, the
Ukrainian default language, and the `-ua` OTA URL override itself (stock
CrossInk points back at upstream). Use Chytanka's own release process (a
manual `pio run -e ua` build attached to a hand-created release) instead.

## Release notes: language behavior for 1.5.0 upgraders

Every Chytanka release note must explain, in the upgrade section, how the UI
language behaves for a device that was already running CrossInk 1.5.0 (or any
pre-Chytanka stock build) before taking the OTA update:

- The UI **stays in whatever language the device already had saved** —
  `CHYTANKA_DEFAULT_LANGUAGE` only seeds the setting before the settings file
  loads (`src/main.cpp`, ~line 1313); loading the saved settings file
  overwrites it whenever a `"language"` key is present, which it is for any
  device that has already run and saved settings once. Chytanka does not
  force a language switch on upgrade.
- To get the Ukrainian UI, the user switches to **Українська** once from
  Settings, same as switching to any other language.
- The `ua` build only compiles in English + Ukrainian
  (`custom_i18n_builtin_langs = uk`, `platformio.chytanka.ini`). Any other
  saved language (e.g. a device that had been running with French or German
  selected) **falls back to English**, because that language isn't built in.
- OTA update checks follow Chytanka's own releases only
  (`CROSSINK_OTA_RELEASE_URL` points at this fork, not upstream CrossInk) —
  call this out so upgraders don't expect to also see stock CrossInk release
  notes/updates.

## Rebase checklist

After every rebase onto a new upstream CrossInk tag:

1. Diff `[env:default]` in `platformio.ini` against what `[env:ua]` in
   `platformio.chytanka.ini` assumes (`extends = env:default`, and it reuses
   `${env:default.build_flags}` verbatim). A flag added, removed, or renamed
   upstream needs a matching look at whether `ua` should inherit it as-is.
2. Diff `inject_version()` in `scripts/git_branch.py` (~line 163 onward).
   `ua` is intentionally **not** in the `{'default', 'sticky', 'x4-pro',
   'x4-classic'}` set it checks, so it never touches `CROSSINK_VERSION` for
   the `ua` env — confirm that's still true after the rebase (i.e. upstream
   didn't start branching on `env['PIOENV'] == 'ua'` or renaming/broadening
   that set in a way that would now catch it and clobber the version string
   this doc defines above).
3. Reset `[chytanka] revision` to `0` in `platformio.chytanka.ini` if
   `[crossink] version` changed.
4. Check the frozen `#else` copies in `BootActivity.cpp` and
   `SleepActivity.cpp` (`drawDefaultBootLogo()` /
   `renderDefaultSleepScreen()`) against CrossInk's current versions of the
   same functions. These are hand-duplicated, not shared code — if upstream
   changed the stock logo/text layout, decide whether the Chytanka branch
   (`chytanka::drawBrandBlock()`) should follow.
5. Check the language default hook in `src/main.cpp` (~line 1313,
   `#ifdef CHYTANKA_DEFAULT_LANGUAGE`) still sits where it did relative to
   `SETTINGS.loadFromFile()` — it must run strictly before that call for the
   "saved language wins" behavior described above to hold.
6. Do **not** enable `CROSSINK_SHOW_SLEEP_BUILD_INFO` in `[env:ua]`. On the
   Chytanka sleep screen the build-info line CrossInk draws with that flag
   overlaps the brand status line Chytanka draws at `H/2 + 118`
   (`chytanka::drawBrandBlock()` draws its `status` line at `pageHeight / 2 +
   120`, and `SleepActivity.cpp`'s `CROSSINK_SHOW_SLEEP_BUILD_INFO` block sits
   right under it) — it's fine (and used) in `[env:debug]`, just never copy it
   into the Chytanka release env.

## Related

- `platformio.chytanka.ini` — `[chytanka]` revision and the `ua` env.
- `src/network/OtaUpdater.cpp` — OTA version comparator traced above.
- `src/activities/boot_sleep/ChytankaBrand.{h,cpp}` — brand screen, guarded by
  `CHYTANKA` and requires `CROSSINK_OTA_RELEASE_URL` to be set at compile
  time.
- `src/activities/boot_sleep/BootActivity.cpp`,
  `src/activities/boot_sleep/SleepActivity.cpp` — call sites / frozen `#else`
  fallbacks.
- `src/CrossPointSettings.cpp` (`getDefaultDeviceName()`) — Chytanka default
  device name and its length caveats, traced above.
