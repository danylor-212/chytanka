#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

// Ukrainian-text helpers for the EPUB engine: detecting Ukrainian books whose
// metadata language is missing or wrong, and the Ukrainian typography layer.
namespace ukrainian_text {

// True when a dc:language value has the primary subtag "uk" (uk, uk-UA, UK).
bool isUkrainianLanguageTag(const std::string& tag);

// Counts letters in the visible text of raw XHTML fed in arbitrary chunks and
// decides whether it is Ukrainian. Markup, character references and the
// contents of <head>, <style> and <script> are ignored.
//
// Decision (calibrated on 110 Ukrainian, English and crafted Russian,
// pre-1918 Russian and Belarusian samples): at least MIN_CYRILLIC Cyrillic
// letters, Cyrillic at least 60% of all letters, the Ukrainian-only letters
// ґ є і ї (any case) at least 2% of the Cyrillic letters (Ukrainian prose:
// 4.5-8.5%; Russian: 0-0.6%), and at least twice as many of them as the
// Russian/Belarusian-only letters ы э ъ ё (Belarusian uses і but also ы э ё).
class LanguageSniffer {
 public:
  static constexpr size_t DEFAULT_VISIBLE_BYTE_LIMIT = 4096;
  static constexpr uint32_t MIN_CYRILLIC = 100;

  explicit LanguageSniffer(size_t visibleByteLimit = DEFAULT_VISIBLE_BYTE_LIMIT) : limit_(visibleByteLimit) {}

  void feed(const char* data, size_t len);
  bool full() const { return visibleBytes_ >= limit_; }
  bool looksUkrainian() const;

  uint32_t cyrillicLetters() const { return cyrillic_; }
  uint32_t latinLetters() const { return latin_; }
  uint32_t ukrainianLetters() const { return ukrainian_; }
  uint32_t russianLetters() const { return russian_; }

 private:
  void countCodepoint(uint32_t cp);
  void finishTagName();

  size_t limit_;
  size_t visibleBytes_ = 0;
  uint32_t cyrillic_ = 0;
  uint32_t latin_ = 0;
  uint32_t ukrainian_ = 0;
  uint32_t russian_ = 0;
  bool inTag_ = false;
  bool inEntity_ = false;
  bool readingTagName_ = false;
  bool closingTag_ = false;
  char tagName_[8] = {};
  uint8_t tagNameLen_ = 0;
  uint8_t skipElement_ = 0;  // 0 = none, else index of head/style/script being skipped
  uint32_t pendingCp_ = 0;
  uint8_t pendingBytes_ = 0;
};

// Ukrainian typography, applied to book text at layout time. Every rule
// replaces one codepoint with exactly one codepoint, so visible-text offsets,
// reading positions and KOReader sync stay the same:
//   - ' ’ ‘ ` between two Cyrillic letters -> ʼ (U+02BC)
//   - straight " -> « after a space, an opening bracket, a dash or at the start
//     of a paragraph, otherwise »; typographic quotes are never touched
//   - " - " (hyphen between spaces) and a dialogue "- " opening a paragraph
//     -> "—"
//   - the space after a one-letter word (в у з і й а о, any case), the space
//     before a dash — and the space after a paragraph-opening dash become
//     U+00A0 NO-BREAK SPACE
// Text arrives in chunks (expat character data); the previous two codepoints
// carry over between chunks, but look-ahead stops at the chunk end, so a rule
// that needs the next character is skipped at a chunk boundary.
class Typography {
 public:
  // Starts a new paragraph: the next character follows a boundary.
  void resetBlock() {
    prev_ = 0;
    prevPrev_ = 0;
  }
  // Appends the transformed text to `out` (cleared first).
  void apply(const char* text, size_t len, std::string& out);

 private:
  uint32_t prev_ = 0;
  uint32_t prevPrev_ = 0;
};

}  // namespace ukrainian_text
