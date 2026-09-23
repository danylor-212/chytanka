#include "LocaleFormat.h"

#include <I18n.h>

#include <cstdio>

namespace LocaleFormat {

DurationPatterns durationPatterns(const DurationStyle style) {
  switch (style) {
    case DurationStyle::Compact:
      return {tr(STR_DURATION_LESS_THAN_MIN_SHORT), tr(STR_DURATION_MIN_SHORT_FMT), tr(STR_DURATION_H_FMT),
              tr(STR_DURATION_H_MIN_SHORT_FMT), false};
    case DurationStyle::Estimate:
      return {tr(STR_STATS_LESS_THAN_MIN), tr(STR_DURATION_MIN_FMT), tr(STR_DURATION_H_FMT),
              tr(STR_DURATION_H_MIN_SHORT_FMT), false};
    case DurationStyle::Carousel:
      return {tr(STR_STATS_LESS_THAN_MIN), tr(STR_DURATION_MIN_SHORT_FMT), tr(STR_DURATION_H_FMT),
              tr(STR_DURATION_H_MIN_SHORT_FMT), true};
    case DurationStyle::Long:
    default:
      return {tr(STR_STATS_LESS_THAN_MIN), tr(STR_DURATION_MIN_FMT), tr(STR_DURATION_H_FMT), tr(STR_DURATION_H_MIN_FMT),
              true};
  }
}

SecondsPatterns secondsPatterns() {
  return {tr(STR_DURATION_SEC_SHORT_FMT), tr(STR_DURATION_MIN_SHORT_FMT), tr(STR_DURATION_MIN_SEC_SHORT_FMT)};
}

void formatDuration(const uint32_t seconds, char* buf, const size_t len, const DurationStyle style,
                    const DurationRounding rounding) {
  formatDurationWith(durationPatterns(style), seconds, rounding, false, buf, len);
}

void formatSeconds(const uint32_t seconds, char* buf, const size_t len) {
  formatSecondsWith(secondsPatterns(), seconds, buf, len);
}

void localizeDecimalSeparator(char* buf) { applyDecimalSeparator(buf, tr(STR_DECIMAL_SEPARATOR)); }

void formatDecimal(const double value, const int decimals, char* buf, const size_t len) {
  if (buf == nullptr || len == 0) return;
  snprintf(buf, len, "%.*f", decimals, value);
  localizeDecimalSeparator(buf);
}

}  // namespace LocaleFormat
