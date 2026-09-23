#pragma once

#include <cstddef>
#include <cstdint>

#include "DurationFormat.h"

// Locale-aware number, unit and duration formatting for UI text. Thin glue
// that feeds the active UI language's patterns into DurationFormat.h.
namespace LocaleFormat {

using DurationStyle = ::DurationStyle;

DurationPatterns durationPatterns(DurationStyle style);
SecondsPatterns secondsPatterns();

void formatDuration(uint32_t seconds, char* buf, size_t len, DurationStyle style = DurationStyle::Long,
                    DurationRounding rounding = DurationRounding::Floor);

// Short interval such as a timeout or idle threshold: "45s", "2m", "2m 30s".
void formatSeconds(uint32_t seconds, char* buf, size_t len);

// Swaps printf's '.' decimal points (a '.' between two digits) in buf for the
// UI language's decimal separator.
void localizeDecimalSeparator(char* buf);

// printf("%.*f") with the UI language's decimal separator.
void formatDecimal(double value, int decimals, char* buf, size_t len);

// Month names for month 1-12 ("" when out of range). The full name is the form
// used next to a day number (Ukrainian genitive: "31 грудня").
const char* monthShortName(uint8_t month);
const char* monthFullName(uint8_t month);

// Day and abbreviated month in the UI language's order: "Dec 31", "31 груд.".
void formatShortDate(uint8_t day, uint8_t month, char* buf, size_t len);

// Expands a date pattern such as tr(STR_DATE_LONG_MDY_PATTERN) with the UI
// language's month names. See formatDatePattern() for the tokens.
void formatLongDate(const char* pattern, uint16_t year, uint8_t month, uint8_t day, char* buf, size_t len);

// Replaces the " AM"/" PM" suffix of a 12-hour clock string with the UI
// language's marker. Other text is left alone.
void localizeMeridiem(char* buf, size_t len);

}  // namespace LocaleFormat
