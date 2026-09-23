#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>

// Pure duration formatting shared by the reading-stats surfaces. The caller
// passes the (translated) printf patterns, so this header has no I18n
// dependency and runs in host tests.
struct DurationPatterns {
  const char* lessThanMinute;  // plain text, e.g. "< 1 min"
  const char* minutes;         // one %lu (minutes), e.g. "%lu min"
  const char* hours;           // one %lu (hours), e.g. "%luh"
  const char* hoursMinutes;    // two %lu (hours, minutes), e.g. "%luh %lu min"
  bool keepZeroMinutes;        // "2h 0 min" instead of "2h"
};

enum class DurationRounding : uint8_t { Floor, Nearest };

// Formats seconds as "< 1 min", "45 min", "3h" or "3h 5 min". A zero minute
// part is omitted unless keepZeroMinutes is set. hoursOnly drops the minute
// part for tight slots.
inline void formatDurationWith(const DurationPatterns& patterns, const uint32_t seconds,
                               const DurationRounding rounding, const bool hoursOnly, char* buf, const size_t len) {
  if (buf == nullptr || len == 0) return;
  if (seconds < 60) {
    snprintf(buf, len, "%s", patterns.lessThanMinute);
    return;
  }
  const uint32_t totalMinutes = rounding == DurationRounding::Nearest ? (seconds + 30U) / 60U : seconds / 60U;
  const auto hours = static_cast<unsigned long>(totalMinutes / 60U);
  const auto minutes = static_cast<unsigned long>(totalMinutes % 60U);
  if (hours == 0) {
    snprintf(buf, len, patterns.minutes, minutes);
  } else if (hoursOnly || (minutes == 0 && !patterns.keepZeroMinutes)) {
    snprintf(buf, len, patterns.hours, hours);
  } else {
    snprintf(buf, len, patterns.hoursMinutes, hours, minutes);
  }
}

// Formats a duration, then retries without the minute part when measure(buf)
// is wider than maxWidth ("12h 55 min" -> "12h"). maxWidth <= 0 disables the
// guard.
template <typename Measure>
void formatDurationToFit(const DurationPatterns& patterns, const uint32_t seconds, const DurationRounding rounding,
                         const int maxWidth, Measure&& measure, char* buf, const size_t len) {
  formatDurationWith(patterns, seconds, rounding, false, buf, len);
  if (maxWidth > 0 && measure(buf) > maxWidth) {
    formatDurationWith(patterns, seconds, rounding, true, buf, len);
  }
}

struct SecondsPatterns {
  const char* seconds;         // one %lu, e.g. "%lus"
  const char* minutes;         // one %lu, e.g. "%lum"
  const char* minutesSeconds;  // two %lu (minutes, seconds), e.g. "%lum %lus"
};

// Formats a short interval such as a timeout: "45s", "2m" or "2m 30s".
inline void formatSecondsWith(const SecondsPatterns& patterns, const uint32_t seconds, char* buf, const size_t len) {
  if (buf == nullptr || len == 0) return;
  const auto minutes = static_cast<unsigned long>(seconds / 60U);
  const auto remainder = static_cast<unsigned long>(seconds % 60U);
  if (minutes == 0) {
    snprintf(buf, len, patterns.seconds, remainder);
  } else if (remainder == 0) {
    snprintf(buf, len, patterns.minutes, minutes);
  } else {
    snprintf(buf, len, patterns.minutesSeconds, minutes, remainder);
  }
}

// Replaces the '.' decimal point that printf emits with the locale's
// separator. Only single-byte separators are supported; anything else leaves
// the text unchanged.
inline void applyDecimalSeparator(char* buf, const char* separator) {
  if (buf == nullptr || separator == nullptr || separator[0] == '\0' || separator[1] != '\0' || separator[0] == '.') {
    return;
  }
  for (char* p = buf; *p != '\0'; ++p) {
    if (*p == '.') *p = separator[0];
  }
}
