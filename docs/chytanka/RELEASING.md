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
  `getEffectiveDeviceName()`/`getDefaultDeviceName()` at all. They are fixed
  strings, Chytanka's own under `CHYTANKA` (see "Network names" below); the
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

## Embedded quote cards (default sleep screen)

Under `CHYTANKA`, the **Dark** and **Light** sleep screen modes (CrossInk's
"default" screen, `SleepActivity::renderDefaultSleepScreen()`) show one of 50
quote cards compiled into the firmware instead of the logo block. Every other
mode (Custom / Cover / Overlay / stats / Quick Resume ...) is unchanged; the
modes that fall back to the default screen when they have nothing to show
(Custom with no images, Cover outside a book, ...) now fall back to a card.
The boot screen keeps the logo. **Light** shows the card as designed; **Dark**
(the default setting) shows it inverted, light text on black, by swapping gray
levels (0<->3, 1<->2) in every render pass so the gray planes stay correct
(inverting the finished B/W framebuffer would not invert them); on the X3 the
side margins are black too. The **cover filter** setting applies on top,
exactly as for an SD sleep image (black & white / inverted drop the gray
passes; inverted B/W in Dark therefore ends up light).

In the settings UI and the web settings page these two modes are labelled
«Цитати — світла тема» / «Цитати — темна тема» ("Quotes — light" / "Quotes —
dark" in English) by `chytanka::settingOptionLabelOverride()`
(`src/activities/settings/ChytankaSettingLabels.h`), keyed on the setting's
JSON key `sleepScreen`. The strings live there rather than in the translation
files, so stock CrossInk keeps «Темний» / «Світлий».

- **Source of truth:** the brand repo — `brand/quotes/embedded_ids.json` (the
  50 ids, written and validated by `brand/quotes/select_embedded.py`: max 3
  per author, at least 20 authors, all reading-themed quotes and Shevchenko
  id 102 included) and the rendered cards `brand/cards/public/q###.bmp`
  (`make_cards.py`, 480x800, gray levels 0/85/170/255 only).
- **Regenerate** after changing the selection or re-rendering cards, then
  commit both generated files:

  ```
  python3 scripts/chytanka/gen_embedded_quotes.py --brand <path>/chytanka/brand
  # or --render to re-render through make_cards.render() (needs cairosvg)
  ```

  It writes `src/images/ChytankaQuoteCards.{h,cpp}` (never edit them by hand;
  the tables sit between `clang-format off/on`, so the formatter leaves them
  alone) and refuses cards with non-native gray levels, a wrong size, or
  anything drawn in rows 0-3 / 796-799 (cut off on the X3). The header records
  the zlib version used; regenerating with another zlib may change the bytes
  but not the decoded cards. Up to 64 cards fit the picker's mask.
- **Format:** per card, 2bpp rows (0 black .. 3 white, leftmost pixel in the
  high bits, as `Bitmap::readNextRow()` hands rows to `GfxRenderer`), one raw
  deflate stream compressed with a 512-byte window, plus a CRC-32 of the
  decoded rows. About 6.2 KB per card, ~311 KB of flash for 50.
- **Rendering** (`ChytankaQuoteSleep.cpp`): no memory-file abstraction exists
  for `Bitmap` (it reads an SD-backed `HalFile`), so the card is inflated row
  by row with uzlib (already linked for fonts) through a 512-byte ring and
  drawn with the same per-pixel rule as `GfxRenderer::drawBitmap()`, once per
  pass (B/W, then the LSB and MSB gray planes), followed by the same display
  sequence as `renderBitmapSleepScreen()`. Heap: one ~1.9 KB decoder object
  for the duration of the render; the 96 KB decoded card never exists in RAM.
  Decode or CRC failure (or OOM) is detected in the B/W pass, before anything
  reaches the panel, and falls back to the brand block.
- **No repeats:** a shuffle bag persisted in `/.crosspoint/chytanka_quotes.bin`
  (16 bytes, fork-owned, separate from `APP_STATE`'s SD sleep-image history):
  each cycle shows every card once, and a new cycle never starts with the
  previous card. A different card count or a corrupt file restarts the bag.
- **Tests:** `test/chytanka_quote_cards` decodes all cards and checks their
  CRCs (computed by the generator from the source BMPs), rejects corrupt and
  truncated streams, and checks the picker. With `CHYTANKA_BRAND_DIR=<brand>`
  it also compares every card with its BMP pixel by pixel;
  `CHYTANKA_QUOTE_DUMP_DIR=<dir>` writes the decoded cards as PGM files.
- On an X3 (528x792 portrait) the 480x800 card is centred unscaled: 24 px
  white side margins, 4 blank rows clipped top and bottom.

## Fonts, keyboards and wake screen (`CHYTANKA`)

- **Reader fonts.** Only families with full Latin and full Ukrainian Cyrillic
  (А–Я а–я Ґґ Єє Іі Її ’ « ») ship. Lexend Deca (partial Cyrillic) is not
  compiled in: `CrossPointSettings::PICKER_BUILTIN_FONTS` lists Bitter only and
  `main.cpp` skips the Lexend `EpdFont` objects, so the linker drops its
  headers (~583 KB). Value 0 stays reserved in the `FONT_FAMILY` enum, so saved
  settings and per-book caches load; `availableBuiltinFont()` maps it to
  Bitter everywhere (settings load, per-book settings, font ID lookup).
- **SD font catalogue.** The manifest is CrossInk's remote `fonts.json`, so
  `FontDownloadActivity.cpp` hides families client-side
  (`chytankaHidesFontFamily()`): a list checked against each family's source
  TTF cmap, plus "no Cyrillic in the manifest languages" for families added
  later. Re-check the list when the catalogue changes (`sd-fonts.yaml`).
- **Keyboards.** `keyboard_layouts::AVAILABLE_BITS` keeps English and
  Ukrainian. Persisted bit positions are unchanged (bit 0 English, bit 5
  Ukrainian); other bits in a saved `keyboardLayouts` are ignored. The layout
  tables themselves live in `freeink-sdk` (FreeInkUI) and are still linked;
  dropping them (~10.6 KB) needs an SDK-side switch.
- **Wake screen.** On a power-button wake CrossInk skips the boot splash and
  goes from the sleep image straight to Home/the reader (the driver turns the
  first paint into a HALF refresh; with the sunlight fading fix it first
  clears the panel white). Under `CHYTANKA` the splashless wake paints the
  Chytanka boot screen with that HALF refresh instead, and Home/the reader land
  with a FAST refresh over it (`main.cpp`, `BootResume::SplashlessWake`). Quick
  Resume frames keep their own path. The log line
  `Wake: Chytanka boot screen painted in N ms` times it on a device.

## Built-in OPDS catalogue (`CHYTANKA`)

`OpdsServerStore` adds «Читанка — Книжки»
(`https://danylor-212.github.io/chytanka-books/opds/index.xml`) at the top of
the OPDS server list once per device: on a fresh device and on one whose saved
`opds.json` predates it. It is an ordinary entry the user can edit or delete.
`opds.json` records `"chytankaCatalogueSeeded": true` after the first add, so a
deleted catalogue does not come back; an entry with the same URL added by hand
counts as present. Stock CrossInk ignores (and drops) that key. If the list is
full (8 servers) nothing is added and the flag stays unset, so it is tried
again once there is room.

## Network names (`CHYTANKA`)

`src/network/ChytankaNetworkNames.h` holds the names; each call site keeps
CrossInk's value in its `#else` branch.

| What | Chytanka | Stock CrossInk | Where |
| --- | --- | --- | --- |
| mDNS hostname | `chytanka` (`http://chytanka.local/`) | `crosspoint` | `CrossPointWebServerActivity.cpp` (`AP_HOSTNAME`, also the URL and QR code on screen), `CalibreConnectActivity.cpp` |
| Hotspot SSID | `Chytanka` | `CrossPoint-Reader` | `CrossPointWebServerActivity.cpp` (`AP_SSID`, also shown and in the Wi-Fi QR code) |
| DHCP hostname (joined network) | `Chytanka-<MAC12>` | `CrossPoint-Reader-<MAC12>` | `WifiSelectionActivity.cpp` |
| HTTP User-Agent | `Chytanka/<version> (CrossInk)` | `CrossInk-ESP32-<version>` | `HttpDownloader.cpp` (both clients), `OtaUpdater.cpp` |

The hotspot SSID has no MAC suffix, like stock. `Chytanka-` + 12 hex digits
is 21 bytes, under the 32-byte limit for both an SSID and an esp_netif
hostname. Left alone on purpose: the UDP discovery reply
`crosspoint (on <hostname>);<port>` in `CrossPointWebServer.cpp` (a protocol
string companion apps match on) and the NVS namespace `crosspoint`. No
translation string mentions the hostname; CrossInk's own docs under `docs/`
still say `crosspoint.local`.

## Web portal in Ukrainian (`CHYTANKA`)

The portal pages are CrossInk's static English HTML/JS (`web/`), gzipped into
flash by `scripts/build_web.py`. Chytanka keeps them and translates in the
browser:

- `scripts/chytanka/build_web_chytanka.py` (pre-script of the `ua` and
  `ua-simulator*` envs only) imports `build_web.py`, composes the same four
  pages with the Chytanka header (inline SVG logo, «Читанка» wordmark), the
  footer «Читанка · based on CrossInk · Open Source» and a
  `<script src="/i18n.js">` before each page's script, and writes them to
  `src/network/html/chytanka/` under the stock identifiers.
  `CrossPointWebServer.cpp` includes those instead of the stock headers under
  `#ifdef CHYTANKA`. The script fails the build if `base.html`'s `<h1>`,
  footer or `{{ script }}` slot changes, so a rebase cannot silently ship
  CrossInk's chrome.
- `/i18n.js` (`src/network/ChytankaWebI18n.cpp`) serves `web/chytanka/uk.js`
  or `en.js` plus the translator `web/chytanka/i18n.js`, chosen by the device
  UI language at request time (`no-cache` + per-language ETag, so switching
  the language takes effect on reload).
- The translator replaces text nodes and `placeholder`/`title`/`aria-label`
  by exact match on the trimmed English text (a leading emoji/symbol prefix
  is kept), uses regular expressions for messages with numbers or names
  (Ukrainian plurals for the folder summary), watches the DOM for everything
  the page scripts add later, and wraps `alert`/`confirm`/`prompt`. File,
  folder, font and network names are never translated (`NO_TRANSLATE`).
  Settings names and options already come from the device in the UI language.
- Not translated: the EPUB optimizer's detailed conversion log, messages the
  device sends as plain text (mostly errors), and the browser's own file
  picker ("Choose files"), which follows the browser language.
- **Rebase:** any English string CrossInk changes in `web/pages/*` silently
  stays English. Diff `web/` after a rebase and update `uk.js`.
- Flash: the Chytanka pages are ~1.7 KB (gz) larger than stock, the tables +
  translator ~7.7 KB (uk 6.3 KB, en 1.4 KB); the `ua` image grew 19,024 bytes
  in 1.6.0.2 for all four UX items together.

## First-run welcome (`CHYTANKA`)

`chytanka::welcomeScreenNeeded()` (`src/activities/home/ChytankaWelcomeActivity.cpp`)
runs in `main.cpp`'s routing just before the normal Home/reader branch (not
on silent/network/crash boots) and decides with `decideWelcome()`
(`ChytankaWelcome.h`, host-tested):

- marker `/.crosspoint/chytanka_welcome.txt` present: nothing;
- no settings file (`crossink-settings.json`, CrossPoint's `settings.json`,
  `settings.bin[.bak]`): a fresh card already has Chytanka's defaults, write
  the marker silently;
- traces of an earlier Chytanka (`chytanka_quotes.bin`, or
  `"chytankaCatalogueSeeded"` in `opds.json`): marker, no screen;
- UI already Ukrainian and reading settings already Bitter + hyphenation on +
  anti-aliasing off: marker, no screen;
- otherwise the welcome replaces Home for this boot.

The screen is bilingual (Ukrainian first, English below), offers only what
differs (interface language; Bitter + hyphenation + no text anti-aliasing),
each as a tick box, with «Застосувати» (focused) and «Пізніше» buttons. Keys:
Up/Down (side or Left/Right) move, Confirm toggles a box or presses a button,
Back = «Пізніше»; button hints follow the current UI language. Input is
ignored until all buttons have been released once, so a key held through boot
cannot answer it. Either answer writes the marker (`applied`/`later`) and goes
Home. Applying the reading settings changes the global defaults only: books
with their own reader settings file keep them (they were chosen for that book,
and rewriting every book's binary settings at boot is SD time for little
gain); an SD font selection is cleared and its point size snapped to Bitter's
like the font picker does.

## Reading line on the quote card (`CHYTANKA`)

`SleepActivity::renderDefaultSleepScreen()` passes the open (or last read)
book's title and progress (`recentBookForPath()`,
`RecentBookProgress::loadPercent()`; nothing when the book file is gone) to
`renderQuoteCardSleepScreen()`, which draws «Читаєте: <title> · 25%»
("Reading: ..." in any other UI language) at x = 44, ending 14 px above the
card's brand footer (y = H − 62): rows 701–721 on the X4, between the lowest
quote text of all 50 cards (y = 683) and the footer (739), clear of rows
796–799. `buildReadingLine()` (`ChytankaReadingLine.h`, host-tested) shortens
the title code point by code point with "…" to fit 392 px, keeping the label
and percentage. Style: Bitter 10 italic in the attribution's level (dark gray;
light gray on the dark card). It is drawn in every pass: ink in the B/W pass,
and the plane bits `grayPlanePixel()` gives for its level in the gray passes
(relative planes via a brief switch to B/W mode, since text in a gray render
mode only draws anti-aliasing pixels). With a B/W cover filter there are no
gray passes, so it is drawn in full ink instead.

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
   "saved language wins" behavior described above to hold. The same applies
   to the `#ifdef CHYTANKA` hyphenation default right below it: confirm
   `fromJson()` still falls back to the in-memory value for
   `hyphenationEnabled`. The Bitter default comes from
   `CrossPointSettings::DEFAULT_FONT_FAMILY` (see "Fonts, keyboards and wake
   screen" above); check that every place that reads `fontFamily` still goes
   through `availableBuiltinFont()` / `builtinFontPickerIndex()`.
6. `CROSSINK_SHOW_SLEEP_BUILD_INFO` (used by `[env:debug]`, never needed in
   `[env:ua]`): stock CrossInk draws that line at `H/2 + 118`, which would
   land on Chytanka's larger brand block (240x240 logo lifted 40 px above
   centre, then «Читанка» in UI_12 bold, credit and status in SMALL — see the
   layout constants in `chytanka::drawBrandBlock()`). Under `CHYTANKA`,
   `SleepActivity::renderDefaultSleepScreen()` moves the line to `H - 30`
   (CrossInk's boot-screen version slot; the Chytanka boot and wake screens
   show the version inside the brand block instead, «версія 1.6.0.0-ua»). If upstream moves or restyles that
   block, keep the `#ifdef CHYTANKA` branch in step.
7. Check `SleepActivity::renderBitmapSleepScreen()` against the display
   sequence copied into `chytanka::renderQuoteCardSleepScreen()`
   (`ChytankaQuoteSleep.cpp`: cover filter, Absolute/Direct grayscale base,
   LSB/MSB planes) and the per-pixel rule in `GfxRenderer::drawBitmap()`
   against `drawCardPass()`. If upstream changed either, mirror it so the
   embedded cards keep looking exactly like an SD sleep BMP.

8. Web portal: diff `web/` and re-check `web/chytanka/uk.js` (see "Web
   portal in Ukrainian"); `build_web_chytanka.py` fails loudly if the header,
   footer or script slot in `base.html` moved.

## Related

- `platformio.chytanka.ini` — `[chytanka]` revision and the `ua` env.
- `src/network/OtaUpdater.cpp` — OTA version comparator traced above.
- `src/activities/boot_sleep/ChytankaBrand.{h,cpp}` — brand screen, guarded by
  `CHYTANKA` and requires `CROSSINK_OTA_RELEASE_URL` to be set at compile
  time.
- `src/activities/boot_sleep/BootActivity.cpp`,
  `src/activities/boot_sleep/SleepActivity.cpp` — call sites / frozen `#else`
  fallbacks.
- `src/activities/boot_sleep/ChytankaQuote{Sleep,Decoder}.{h,cpp}`,
  `src/images/ChytankaQuoteCards.{h,cpp}` (generated),
  `scripts/chytanka/gen_embedded_quotes.py` — embedded quote cards.
- `src/CrossPointSettings.cpp` (`getDefaultDeviceName()`) — Chytanka default
  device name and its length caveats, traced above.
