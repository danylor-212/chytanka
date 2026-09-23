#pragma once

#include <Utf8.h>

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <cstring>

// Pure date/time text helpers; the translated patterns come from the caller
// so they run in host tests.

struct DatePatternValues {
  unsigned day;
  unsigned year;
  const char* monthShort;  // "Dec"
  const char* monthFull;   // "December"
};

// Expands a date pattern so each language can choose its own order:
// {d} day, {dd} two-digit day, {y} year, {m} short month, {M} full month.
// Other text is copied as is.
inline void formatDatePattern(const char* pattern, const DatePatternValues& values, char* buf, const size_t len) {
  if (buf == nullptr || len == 0) return;
  size_t out = 0;
  const auto append = [&](const int written) {
    if (written > 0) out = std::min(len - 1, out + static_cast<size_t>(written));
  };
  for (const char* p = pattern != nullptr ? pattern : ""; *p != '\0' && out + 1 < len;) {
    if (strncmp(p, "{dd}", 4) == 0) {
      append(snprintf(buf + out, len - out, "%02u", values.day));
      p += 4;
    } else if (strncmp(p, "{d}", 3) == 0) {
      append(snprintf(buf + out, len - out, "%u", values.day));
      p += 3;
    } else if (strncmp(p, "{y}", 3) == 0) {
      append(snprintf(buf + out, len - out, "%u", values.year));
      p += 3;
    } else if (strncmp(p, "{m}", 3) == 0) {
      append(snprintf(buf + out, len - out, "%s", values.monthShort ? values.monthShort : ""));
      p += 3;
    } else if (strncmp(p, "{M}", 3) == 0) {
      append(snprintf(buf + out, len - out, "%s", values.monthFull ? values.monthFull : ""));
      p += 3;
    } else {
      buf[out++] = *p++;
    }
  }
  buf[out] = '\0';
  // A long month name may have been cut mid-character.
  utf8TrimIncompleteTail(buf);
}

// Swaps the " AM"/" PM" suffix of a 12-hour clock string for the given
// markers. Text without that suffix is left alone.
inline void replaceMeridiem(char* buf, const size_t len, const char* am, const char* pm) {
  if (buf == nullptr || len == 0) return;
  const size_t textLen = strlen(buf);
  if (textLen < 3) return;
  char* suffix = buf + textLen - 3;
  const bool isAm = strcmp(suffix, " AM") == 0;
  if (!isAm && strcmp(suffix, " PM") != 0) return;
  snprintf(suffix, len - (textLen - 3), " %s", isAm ? am : pm);
}
