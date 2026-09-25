#pragma once

// Fork-only («Читанка» / Chytanka): the "currently reading" line drawn at the
// bottom of the quote sleep card, «Читаєте: <title> · 25%». Hardware-free so
// the native test suite can check the wording and the UTF-8-safe truncation.

#include <string>

namespace chytanka {

// Whole percent to show for a book progress in 0..100, or -1 for none
// (RecentBookProgress reports unknown progress as a negative value). Rounds
// down, so a book reads 100% only once it is actually finished.
inline int readingLinePercent(const float progressPercent) {
  if (!(progressPercent >= 0.0f)) return -1;  // also rejects NaN
  if (progressPercent >= 100.0f) return 100;
  return static_cast<int>(progressPercent);
}

// Removes the last UTF-8 code point (and any spaces left before it).
inline void popLastCodepoint(std::string& text) {
  while (!text.empty() && (static_cast<unsigned char>(text.back()) & 0xC0) == 0x80) text.pop_back();
  if (!text.empty()) text.pop_back();
  while (!text.empty() && text.back() == ' ') text.pop_back();
}

// Builds the line for `title` and `percent` (-1: leave the percentage out) in
// Ukrainian or English. `fits(const std::string&)` reports whether a candidate
// fits the available width; the title is shortened code point by code point
// with a trailing "…" until it does, while the label and the percentage stay
// intact. Returns an empty string when there is no title or not even
// "<label> …" fits.
template <typename Fits>
std::string buildReadingLine(const std::string& title, const int percent, const bool ukrainian, Fits&& fits) {
  if (title.empty()) return {};
  const std::string prefix = ukrainian ? "Читаєте: " : "Reading: ";
  std::string suffix;
  if (percent >= 0) suffix = " · " + std::to_string(percent) + "%";

  std::string line = prefix + title + suffix;
  if (fits(line)) return line;

  std::string shortened = title;
  while (!shortened.empty()) {
    popLastCodepoint(shortened);
    line = prefix + shortened + "…" + suffix;
    if (!shortened.empty() && fits(line)) return line;
  }
  return {};
}

}  // namespace chytanka
