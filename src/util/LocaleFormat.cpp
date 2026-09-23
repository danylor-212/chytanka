#include "LocaleFormat.h"

#include <I18n.h>

#include <cstdio>

#include "DateTextFormat.h"
#include "NumberFormat.h"

namespace LocaleFormat {

DurationPatterns durationPatterns(const DurationStyle style) {
  switch (style) {
#define LOCALE_FORMAT_STYLE_CASE(styleName, lessThanMinute, minutes, hours, hoursMinutes, keepZeroMinutes) \
  case DurationStyle::styleName:                                                                           \
    return {tr(lessThanMinute), tr(minutes), tr(hours), tr(hoursMinutes), keepZeroMinutes};
    DURATION_STYLE_KEYS(LOCALE_FORMAT_STYLE_CASE)
#undef LOCALE_FORMAT_STYLE_CASE
  }
  return durationPatterns(DurationStyle::Long);
}

SecondsPatterns secondsPatterns() {
#define LOCALE_FORMAT_SECONDS(seconds, minutes, minutesSeconds) return {tr(seconds), tr(minutes), tr(minutesSeconds)};
  SECONDS_STYLE_KEYS(LOCALE_FORMAT_SECONDS)
#undef LOCALE_FORMAT_SECONDS
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

namespace {
constexpr StrId kMonthShort[] = {StrId::STR_MONTH_JAN_SHORT, StrId::STR_MONTH_FEB_SHORT, StrId::STR_MONTH_MAR_SHORT,
                                 StrId::STR_MONTH_APR_SHORT, StrId::STR_MONTH_MAY_SHORT, StrId::STR_MONTH_JUN_SHORT,
                                 StrId::STR_MONTH_JUL_SHORT, StrId::STR_MONTH_AUG_SHORT, StrId::STR_MONTH_SEP_SHORT,
                                 StrId::STR_MONTH_OCT_SHORT, StrId::STR_MONTH_NOV_SHORT, StrId::STR_MONTH_DEC_SHORT};
constexpr StrId kMonthFull[] = {StrId::STR_MONTH_JAN_FULL, StrId::STR_MONTH_FEB_FULL, StrId::STR_MONTH_MAR_FULL,
                                StrId::STR_MONTH_APR_FULL, StrId::STR_MONTH_MAY_FULL, StrId::STR_MONTH_JUN_FULL,
                                StrId::STR_MONTH_JUL_FULL, StrId::STR_MONTH_AUG_FULL, StrId::STR_MONTH_SEP_FULL,
                                StrId::STR_MONTH_OCT_FULL, StrId::STR_MONTH_NOV_FULL, StrId::STR_MONTH_DEC_FULL};
}  // namespace

const char* monthShortName(const uint8_t month) {
  return month >= 1 && month <= 12 ? I18N.get(kMonthShort[month - 1]) : "";
}

const char* monthFullName(const uint8_t month) {
  return month >= 1 && month <= 12 ? I18N.get(kMonthFull[month - 1]) : "";
}

void formatShortDate(const uint8_t day, const uint8_t month, char* buf, const size_t len) {
  formatDatePattern(tr(STR_SHORT_DATE_PATTERN), {day, 0, monthShortName(month), monthFullName(month)}, buf, len);
}

void formatLongDate(const char* pattern, const uint16_t year, const uint8_t month, const uint8_t day, char* buf,
                    const size_t len) {
  formatDatePattern(pattern, {day, year, monthShortName(month), monthFullName(month)}, buf, len);
}

void localizeMeridiem(char* buf, const size_t len) { replaceMeridiem(buf, len, tr(STR_AM), tr(STR_PM)); }

}  // namespace LocaleFormat
