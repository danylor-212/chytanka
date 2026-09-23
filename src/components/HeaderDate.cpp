#include "HeaderDate.h"

#include <GfxRenderer.h>
#include <HalClock.h>
#include <I18n.h>

#include <algorithm>
#include <cstddef>

#include "CrossPointSettings.h"
#include "components/themes/BaseTheme.h"
#include "fontIds.h"
#include "util/LocaleFormat.h"

namespace {
constexpr int kHeaderDateRightInset = 12;
constexpr int kHeaderDateBottomGap = 10;
// Room for a localised month name ("31 листопада" is 23 bytes of UTF-8).
constexpr size_t kHeaderDateBufferSize = 32;

// Reads the fixed "YYYY-MM-DD" text HalClock::formatDate() emits. Parsed by
// hand: sscanf would pull the whole libc scanf engine (~9 KB) into flash.
[[maybe_unused]] bool parseIsoDate(const char* text, unsigned& year, unsigned& month, unsigned& day) {
  const auto digits = [text](const int from, const int count, unsigned& out) {
    out = 0;
    for (int i = from; i < from + count; ++i) {
      if (text[i] < '0' || text[i] > '9') return false;
      out = out * 10 + static_cast<unsigned>(text[i] - '0');
    }
    return true;
  };
  return text != nullptr && digits(0, 4, year) && text[4] == '-' && digits(5, 2, month) && text[7] == '-' &&
         digits(8, 2, day) && text[10] == '\0';
}

[[maybe_unused]] bool isLongDateFormat(const HalClock::DateFormat format) {
  return format == HalClock::MONTH_DAY_YEAR_LONG || format == HalClock::DAY_MONTH_YEAR_LONG ||
         format == HalClock::MONTH_DAY_LONG || format == HalClock::DAY_MONTH_LONG;
}

char dateSeparatorChar() {
  switch (SETTINGS.dateSeparator) {
    case CrossPointSettings::DATE_SEPARATOR_PERIOD:
      return '.';
    case CrossPointSettings::DATE_SEPARATOR_HYPHEN:
      return '-';
    case CrossPointSettings::DATE_SEPARATOR_SLASH:
    default:
      return '/';
  }
}

bool formatHeaderDateImpl(char* buf, const size_t len) {
  if (!halClock.isAvailable()) return false;
  if (!SETTINGS.clockDateHasBeenSynced) return false;
#if defined(SIMULATOR) && !defined(CROSSPOINT_SIMULATOR_HAS_DATE_FORMAT)
  // Keep compatibility with older downloaded simulator libraries.
  return halClock.formatDate(buf, len, SETTINGS.clockUtcOffsetQ);
#elif defined(SIMULATOR) && !defined(CROSSPOINT_SIMULATOR_HAS_DATE_SEPARATOR)
  // Older simulator libraries support date formats but always emit slashes.
  if (!halClock.formatDate(buf, len, SETTINGS.clockUtcOffsetQ,
                           static_cast<HalClock::DateFormat>(SETTINGS.dateFormat))) {
    return false;
  }
  const char separator = dateSeparatorChar();
  if (separator != '/') {
    for (char* p = buf; *p != '\0'; ++p) {
      if (*p == '/') *p = separator;
    }
  }
  return true;
#else
  const auto format = static_cast<HalClock::DateFormat>(SETTINGS.dateFormat);
  if (!isLongDateFormat(format)) {
    return halClock.formatDate(buf, len, SETTINGS.clockUtcOffsetQ, format, dateSeparatorChar());
  }
  // The HAL spells month names in English; take the local date from it and
  // name the month in the UI language.
  char numeric[16];
  unsigned year = 0;
  unsigned month = 0;
  unsigned day = 0;
  if (!halClock.formatDate(numeric, sizeof(numeric), SETTINGS.clockUtcOffsetQ, HalClock::YEAR_MONTH_DAY_NUMERIC, '-') ||
      !parseIsoDate(numeric, year, month, day)) {
    return false;
  }
  // Each long Date Format setting maps to a translatable pattern. A language
  // whose natural order is day-first may give the month-first and day-first
  // settings the same pattern (Ukrainian does), which is intended.
  const char* pattern = tr(STR_DATE_LONG_MDY_PATTERN);
  switch (format) {
    case HalClock::DAY_MONTH_YEAR_LONG:
      pattern = tr(STR_DATE_LONG_DMY_PATTERN);
      break;
    case HalClock::MONTH_DAY_LONG:
      pattern = tr(STR_DATE_LONG_MD_PATTERN);
      break;
    case HalClock::DAY_MONTH_LONG:
      pattern = tr(STR_DATE_LONG_DM_PATTERN);
      break;
    default:
      break;
  }
  LocaleFormat::formatLongDate(pattern, static_cast<uint16_t>(year), static_cast<uint8_t>(month),
                               static_cast<uint8_t>(day), buf, len);
  return true;
#endif
}
}  // namespace

bool formatHeaderDateText(char* buffer, const size_t length) { return formatHeaderDateImpl(buffer, length); }

int headerDateReservedWidth(const GfxRenderer& renderer) {
  char dateBuf[kHeaderDateBufferSize];
  if (!formatHeaderDateImpl(dateBuf, sizeof(dateBuf))) return 0;

  return renderer.getTextWidth(UI_10_FONT_ID, dateBuf) + kHeaderDateRightInset;
}

int headerDateLineBottomY(const GfxRenderer&, const ThemeMetrics& metrics, const int headerHeight) {
  const int effectiveHeaderHeight = headerHeight >= 0 ? headerHeight : metrics.headerHeight;
  return metrics.topPadding + effectiveHeaderHeight - kHeaderDateBottomGap;
}

void drawHeaderDate(const GfxRenderer& renderer, const int pageWidth, const ThemeMetrics& metrics,
                    const int headerHeight) {
  drawHeaderDateAtLineBottom(renderer, pageWidth, headerDateLineBottomY(renderer, metrics, headerHeight));
}

void drawHeaderDateAtLineBottom(const GfxRenderer& renderer, const int pageWidth, const int lineBottomY) {
  constexpr int dateFontId = UI_10_FONT_ID;
  drawHeaderDateAtBaseline(renderer, pageWidth,
                           lineBottomY - renderer.getLineHeight(dateFontId) + renderer.getFontAscenderSize(dateFontId));
}

void drawHeaderDateAtBaseline(const GfxRenderer& renderer, const int pageWidth, const int baselineY) {
  char dateBuf[kHeaderDateBufferSize];
  if (!formatHeaderDateImpl(dateBuf, sizeof(dateBuf))) return;

  constexpr int dateFontId = UI_10_FONT_ID;
  const int textWidth = renderer.getTextWidth(dateFontId, dateBuf);
  const int dateX = pageWidth - kHeaderDateRightInset - textWidth;
  const int dateY = baselineY - renderer.getFontAscenderSize(dateFontId);
  renderer.drawText(dateFontId, std::max(0, dateX), std::max(0, dateY), dateBuf);
}
