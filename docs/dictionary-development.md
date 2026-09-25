# Dictionary Development Guide

This guide describes the dictionary implementation currently shipped on this branch. For installation and use, see [dictionary.md](dictionary.md).

## Supported StarDict Files

| File | Required | Purpose |
|------|----------|---------|
| `.dict` | Yes | Uncompressed definition data |
| `.idx` | Yes | Sorted headwords and offsets into `.dict` |
| `.ifo` | Recommended | Metadata and `sametypesequence` used to interpret definition fields |
| `.syn` | Optional | Alternate forms mapped to `.idx` ordinals |
| `.idx.oft` / `.syn.oft` | Optional | Coarse page offsets used to narrow a scan |
| `.idx.oft.cspt` / `.syn.oft.cspt` | Optional | CrossInk prefix indexes used as the fastest lookup path |
| `.qidx` | Generated | Disposable sampled index built by the device when `.idx` has no prepared accelerator |

The device requires an uncompressed `.dict`; it does not read `.dict.dz` or `.syn.dz` directly. Use `scripts/dictionary_tools.py prep` on a computer when decompression is needed or to generate the fastest `.oft`/`.cspt` accelerators. For an uncompressed dictionary without those accelerators, the device automatically generates `.qidx` on first lookup.

Dictionary discovery is implemented by `DictionaryRegistry`. It checks `/.dictionaries` first and `/dictionaries` second, using only the first root directory found. Each child folder must contain exactly one `.idx`, no more than one `.ifo`, and a `.dict` file. Hidden folders and ambiguous folders are skipped.

## Lookup Paths

`Dictionary::locate` searches `.idx` in this order:

1. Use `.idx.oft.cspt` to select a small byte range.
2. Fall back to `.idx.oft` to select an offset page.
3. Fall back to the device-generated `.qidx` sampled index.
4. Fall back to scanning `.idx` from the beginning if the sidecar cannot be read or written.

`Dictionary::locateWithStemVariants` probes every spelling from `Dictionary::lookupKeyVariants` before trying English stems: the selected word, the word after key normalisation (apostrophes ' ` ´ ʼ ‘ ’ become U+2019 ’, combining acute U+0301 is removed), its full lowercase (ASCII, Latin-1/Extended-A and Cyrillic U+0400–U+052F, the same mapping hyphenation uses), a title-case form, and the lowercase form with an ASCII apostrophe.

`Dictionary::locateAltForm` and `Dictionary::resolveAltForm` try the same spellings in `.syn`, using `.syn.oft.cspt`, then `.syn.oft`, then a full `.syn` scan. `locateAltForm` then reads the `.idx` entry directly by ordinal. The lookup controller runs it automatically when `.syn` has a `.syn.oft` or `.syn.oft.cspt` accelerator (`Dictionary::hasIndexedAltForms`), and keeps the confirmation prompt for dictionaries whose `.syn` would need a full scan. `Dictionary::findSimilar` uses `.idx.oft` when available and otherwise uses `.qidx` to scan a bounded neighborhood. If neither index accelerator is usable, spelling suggestions are skipped so a large dictionary cannot block the reader UI; direct lookup still works with the uncompressed `.dict` and `.idx` files.

`.qidx` uses a 20-byte little-endian header containing `QIDX`, format version, sample interval, sample count, and source `.idx` size, followed by the byte offset of every 256th `.idx` entry. It is written through a temporary file and installed only after the complete scan succeeds. A size mismatch or invalid header causes it to be rebuilt.

StarDict files with `idxoffsetbits=64` are parsed, but entries whose definition offset exceeds the device's supported 32-bit range fail safely.

## Definition Rendering

`DictionaryDefinitionActivity` resolves the selected `.dict` byte range and renders one page at a time. `DictHtmlRenderer` streams HTML input, while `DictLayout::Wrapper` wraps styled spans into page lines. Keeping only the current page bounds peak RAM and avoids materializing a large definition as one in-memory document. Definition body text uses the active reader font; headers and controls keep built-in UI fonts. SD-font text is passed through unchanged. Built-in coverage uses the audited, fixed Lexend Deca/Bitter glyph set (including the renderer's `Γ`, `ε`, and `ω` fallbacks), while unsupported IPA, Greek, combining-mark, and punctuation codepoints use the existing approximations.

Paging re-parses the definition from its start. This trades extra sequential reads for predictable memory usage on the ESP32-C3.

Chained lookups use `LookupChain`, which stores compact history positions and page numbers instead of owned copies of every headword. The chain is bounded by `LookupHistory::MAX_VISIBLE_ENTRIES` (currently 50).

## Lookup History

Each EPUB cache stores history at `<cachePath>/dictionary_history.txt`. Lines use `word|STATUS`, where the status is direct, stemmed, alternate form, suggestion, or not found.

The file is append-only and is not automatically truncated. The UI loads only the newest 50 entries. Cache-clear helpers preserve this file as user state.

## Offline CLI

The standard-library-only tool is `scripts/dictionary_tools.py`:

```bash
# Decompress .dict.dz/.syn.dz and generate .oft/.cspt files.
python3 scripts/dictionary_tools.py prep /path/to/dictionary-folder

# Perform an exact lookup.
python3 scripts/dictionary_tools.py lookup /path/to/dictionary-folder apple

# Merge prepared dictionaries.
python3 scripts/dictionary_tools.py merge \
  --source /path/to/dict-a \
  --source /path/to/dict-b \
  --output /path/to/merged-dict
```

`lookup` and `merge` require an uncompressed `.dict`. `merge` writes a prepared output including applicable `.oft` and `.cspt` files.

## Generating Dictionary Fonts

Dictionary definitions use the active reader font, so the SD-card font catalog
also has a dictionary-specific build. The
`lib/EpdFont/scripts/build-dictionary-fonts.py` wrapper uses the family, style,
and size catalog in `lib/EpdFont/scripts/sd-fonts.yaml`, then adds the broad
IPA, combining-mark, and reader ranges needed by dictionary definitions before
delegating to `build-sd-fonts.py`. It packages each generated family as a ZIP.

Install the font-builder dependencies and run it from the CrossInk repository
root:

```bash
python3 -m pip install -r lib/EpdFont/scripts/requirements.txt
python3 lib/EpdFont/scripts/build-dictionary-fonts.py --clean --jobs 2
```

If you are generating your own dictionary fonts, change the output directory so
your files do not mix with the shared catalog output. For example:

```bash
python3 lib/EpdFont/scripts/build-dictionary-fonts.py \
  --output-dir ./generated-dictionary-fonts
```

The `--clean` option removes the selected output directory before building, so
do not use it with the default location unless you intend to rebuild that
catalog.

By default, the generated family folders and ZIPs are written to
`../crossink-fonts/dictionary-fonts`. Unzip a family archive into `/.fonts/` or
`/fonts/` on the SD card, or copy the output to the sibling `crossink-fonts`
repository when publishing the catalog. Use `--output-dir` to choose another
destination.

Useful options:

| Option | Purpose |
|--------|---------|
| `--only FamilyA,FamilyB` | Build only the named families from `sd-fonts.yaml` |
| `--config path/to/catalog.yaml` | Use a different family catalog |
| `--output-dir path` | Write family folders and ZIPs somewhere other than the default |
| `--clean` | Remove the output directory before building, avoiding stale `.cpfont` files |
| `--jobs N` / `-j N` | Limit parallel family builds |
| `--timeout SECONDS` | Set the per-family converter timeout (default: 600) |
| `--verbose` / `-v` | Stream converter output while debugging a build |

The wrapper does not change `sd-fonts.yaml`; it creates a temporary transformed
catalog for the shared builder. If a family is changed or removed, use
`--clean` so stale files cannot be mistaken for current output.

## CrossInk Prefix Index (`.cspt`)

Both `.idx.oft.cspt` and `.syn.oft.cspt` use the same format:

| Offset | Size | Field |
|--------|------|-------|
| 0 | 4 | Magic `CSPT` |
| 4 | 1 | Version (`1`) |
| 5 | 1 | Prefix length (`16`) |
| 6 | 2 | Producer stride (`16`, little-endian) |
| 8 | 4 | Entry count (little-endian) |
| 12... | 20 each | 16-byte, zero-padded prefix plus 4-byte source offset |

The reader binary-searches for the last prefix that sorts before the target, then scans forward until it passes the target. A prefix with no zero padding may be a truncated longer word (16 bytes is only 8 Cyrillic letters), so when such a prefix equals the start of the target the search treats it as not before the target and starts one sample earlier. Invalid or missing `.cspt` data falls back to `.oft`, then to a full scan.

## Key Order And Normalisation

`.idx` and `.syn` must be sorted in StarDict order: compare UTF-8 bytes after folding only ASCII `A`-`Z` to lowercase, and break ties with a plain byte comparison. In Python that is `key=lambda w: (w.encode().lower(), w.encode())` (`scripts/dictionary_tools.py` exposes it as `stardict_sort_key`). Do not sort with `str.lower()`: it folds Cyrillic and other scripts, and the device's binary search then lands in the wrong place. The device does not fold non-ASCII case inside the index comparison; it probes lowercase and title-case spellings instead.

Dictionary builders should store keys already normalised the way the device normalises lookups: use U+2019 ’ for every apostrophe and drop U+0301 stress marks. Keep common words lowercase and proper nouns capitalised.

The header's stride field is currently informational. Producers must continue to emit `16` until the format version and readers are updated together.

## Verification

`test/dictionary_lookup` builds small StarDict fixtures (with and without `.oft`, `.cspt` and `.syn`) and covers case folding, normalisation, the `.cspt` truncated-prefix case and `.syn` resolution. `DictionaryLookupBench <folder>/<stem> [sampleEvery]`, built in the same folder, resolves every headword and `.syn` form of a real dictionary and prints the average file opens, seeks and SD sectors per lookup. For changes:

1. Run `python3 scripts/dictionary_tools.py prep` and `lookup` against a representative StarDict dictionary.
2. Build the simulator with `pio run -e simulator` for reader/UI integration.
3. On hardware, test dictionaries with and without `.oft`/`.cspt`, a `.syn` dictionary, HTML definitions, long definitions, lookup history, chained lookup, and per-book overrides.

Multi-word selection is limited to the currently rendered page. Reducing the reader or definition font size can fit more of a phrase on one page.
