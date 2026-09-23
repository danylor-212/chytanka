#pragma once

#include <cstddef>
#include <cstdint>

#include "DurationFormat.h"

// Locale-aware number, unit and duration formatting for UI text. Thin glue
// that feeds the active UI language's patterns into DurationFormat.h.
namespace LocaleFormat {

// The duration looks each surface has always used (English shown):
//   Long:     "< 1 min", "45 min", "3h 0 min", "3h 5 min"  (stats screens)
//   Compact:  "<1m",     "45m",    "3h",       "3h 5m"     (status bar)
//   Estimate: "< 1 min", "45 min", "3h",       "3h 5m"     (Dashboard time left)
//   Carousel: "< 1 min", "45m",    "3h 0m",    "3h 5m"     (Lyra Carousel)
enum class DurationStyle : uint8_t { Long, Compact, Estimate, Carousel };

DurationPatterns durationPatterns(DurationStyle style);
SecondsPatterns secondsPatterns();

void formatDuration(uint32_t seconds, char* buf, size_t len, DurationStyle style = DurationStyle::Long,
                    DurationRounding rounding = DurationRounding::Floor);

// Short interval such as a timeout or idle threshold: "45s", "2m", "2m 30s".
void formatSeconds(uint32_t seconds, char* buf, size_t len);

// Swaps printf's '.' for the UI language's decimal separator. Only call it on
// purely numeric text: every '.' in buf is replaced.
void localizeDecimalSeparator(char* buf);

// printf("%.*f") with the UI language's decimal separator.
void formatDecimal(double value, int decimals, char* buf, size_t len);

}  // namespace LocaleFormat
