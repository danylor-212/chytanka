#include <gtest/gtest.h>

#include <algorithm>
#include <cstring>
#include <fstream>
#include <map>
#include <string>

#include "util/DurationFormat.h"

namespace {

// Reads the flat `KEY: "value"` translation files so the tests exercise the
// shipped patterns instead of copies of them.
std::map<std::string, std::string> loadTranslations(const char* file) {
  std::map<std::string, std::string> strings;
  std::ifstream in(std::string(TRANSLATIONS_DIR) + "/" + file);
  std::string line;
  while (std::getline(in, line)) {
    const size_t colon = line.find(": \"");
    if (colon == std::string::npos || line.back() != '"') continue;
    std::string raw = line.substr(colon + 3, line.size() - colon - 4);
    std::string value;
    for (size_t i = 0; i < raw.size(); ++i) {
      if (raw[i] == '\\' && i + 1 < raw.size()) {
        const char next = raw[++i];
        value += next == 'n' ? '\n' : next;
      } else {
        value += raw[i];
      }
    }
    strings[line.substr(0, colon)] = value;
  }
  return strings;
}

struct Language {
  std::map<std::string, std::string> strings;
  std::map<std::string, std::string> fallback;

  const char* get(const char* key) const {
    const auto it = strings.find(key);
    if (it != strings.end()) return it->second.c_str();
    return fallback.at(key).c_str();
  }

  // Mirrors LocaleFormat::durationPatterns().
  DurationPatterns longPatterns() const {
    return {get("STR_STATS_LESS_THAN_MIN"), get("STR_DURATION_MIN_FMT"), get("STR_DURATION_H_FMT"),
            get("STR_DURATION_H_MIN_FMT"), true};
  }

  DurationPatterns compactPatterns() const {
    return {get("STR_DURATION_LESS_THAN_MIN_SHORT"), get("STR_DURATION_MIN_SHORT_FMT"), get("STR_DURATION_H_FMT"),
            get("STR_DURATION_H_MIN_SHORT_FMT"), false};
  }

  DurationPatterns estimatePatterns() const {
    return {get("STR_STATS_LESS_THAN_MIN"), get("STR_DURATION_MIN_FMT"), get("STR_DURATION_H_FMT"),
            get("STR_DURATION_H_MIN_SHORT_FMT"), false};
  }

  DurationPatterns carouselPatterns() const {
    return {get("STR_STATS_LESS_THAN_MIN"), get("STR_DURATION_MIN_SHORT_FMT"), get("STR_DURATION_H_FMT"),
            get("STR_DURATION_H_MIN_SHORT_FMT"), true};
  }

  SecondsPatterns secondsPatterns() const {
    return {get("STR_DURATION_SEC_SHORT_FMT"), get("STR_DURATION_MIN_SHORT_FMT"),
            get("STR_DURATION_MIN_SEC_SHORT_FMT")};
  }
};

const Language& english() {
  static const Language lang{loadTranslations("english.yaml"), {}};
  return lang;
}

const Language& ukrainian() {
  static const Language lang{loadTranslations("ukrainian.yaml"), english().strings};
  return lang;
}

// U+202F NARROW NO-BREAK SPACE keeps a number and its unit together.
#define NNBSP "\xE2\x80\xAF"

std::string format(const DurationPatterns& patterns, const uint32_t seconds,
                   const DurationRounding rounding = DurationRounding::Floor, const bool hoursOnly = false) {
  char buf[48];
  formatDurationWith(patterns, seconds, rounding, hoursOnly, buf, sizeof(buf));
  return buf;
}

std::string formatSeconds(const SecondsPatterns& patterns, const uint32_t seconds) {
  char buf[48];
  formatSecondsWith(patterns, seconds, buf, sizeof(buf));
  return buf;
}

}  // namespace

TEST(DurationFormat, EnglishLong) {
  const auto p = english().longPatterns();
  EXPECT_EQ(format(p, 59), "< 1 min");
  EXPECT_EQ(format(p, 60), "1 min");
  EXPECT_EQ(format(p, 45 * 60 + 59), "45 min");
  EXPECT_EQ(format(p, 2 * 3600), "2h 0 min");
  EXPECT_EQ(format(p, 12 * 3600 + 55 * 60), "12h 55 min");
}

// The English outputs below are the ones each surface printed before
// localisation; they must not change.
TEST(DurationFormat, EnglishEstimate) {
  const auto p = english().estimatePatterns();
  EXPECT_EQ(format(p, 20, DurationRounding::Nearest), "< 1 min");
  EXPECT_EQ(format(p, 45 * 60, DurationRounding::Nearest), "45 min");
  EXPECT_EQ(format(p, 3 * 3600, DurationRounding::Nearest), "3h");
  EXPECT_EQ(format(p, 3 * 3600 + 5 * 60, DurationRounding::Nearest), "3h 5m");
}

TEST(DurationFormat, EnglishCarousel) {
  const auto p = english().carouselPatterns();
  EXPECT_EQ(format(p, 20), "< 1 min");
  EXPECT_EQ(format(p, 45 * 60), "45m");
  EXPECT_EQ(format(p, 2 * 3600), "2h 0m");
  EXPECT_EQ(format(p, 2 * 3600 + 5 * 60), "2h 5m");
}

TEST(DurationFormat, OtherLanguagesKeepTheirLessThanMinute) {
  // German has its own "< 1 min" string but none of the new duration keys; the
  // Carousel must keep showing the German text rather than an English one.
  const Language german{loadTranslations("german.yaml"), english().strings};
  EXPECT_STRNE(german.get("STR_STATS_LESS_THAN_MIN"), english().get("STR_STATS_LESS_THAN_MIN"));
  EXPECT_EQ(format(german.carouselPatterns(), 20), german.get("STR_STATS_LESS_THAN_MIN"));
}

TEST(DurationFormat, EnglishCompact) {
  const auto p = english().compactPatterns();
  EXPECT_EQ(format(p, 20, DurationRounding::Nearest), "<1m");
  EXPECT_EQ(format(p, 90, DurationRounding::Nearest), "2m");
  EXPECT_EQ(format(p, 3600 + 5 * 60, DurationRounding::Nearest), "1h 5m");
  EXPECT_EQ(format(p, 3600 + 59 * 60 + 40, DurationRounding::Nearest), "2h");
}

TEST(DurationFormat, UkrainianLong) {
  const auto p = ukrainian().longPatterns();
  EXPECT_EQ(format(p, 30), "< 1" NNBSP "хв");
  EXPECT_EQ(format(p, 45 * 60), "45" NNBSP "хв");
  EXPECT_EQ(format(p, 3 * 3600), "3" NNBSP "год 0" NNBSP "хв");
  EXPECT_EQ(format(p, 12 * 3600 + 55 * 60), "12" NNBSP "год 55" NNBSP "хв");
}

TEST(DurationFormat, UkrainianCompact) {
  const auto p = ukrainian().compactPatterns();
  EXPECT_EQ(format(p, 20, DurationRounding::Nearest), "<1" NNBSP "хв");
  EXPECT_EQ(format(p, 3600 + 5 * 60, DurationRounding::Nearest), "1" NNBSP "год 5" NNBSP "хв");
}

TEST(DurationFormat, RoundingOnlyAffectsMinutes) {
  const auto p = english().longPatterns();
  EXPECT_EQ(format(p, 60 + 40, DurationRounding::Floor), "1 min");
  EXPECT_EQ(format(p, 60 + 40, DurationRounding::Nearest), "2 min");
}

TEST(DurationFormat, HoursOnlyDropsMinutes) {
  EXPECT_EQ(format(english().longPatterns(), 12 * 3600 + 55 * 60, DurationRounding::Floor, true), "12h");
  EXPECT_EQ(format(ukrainian().longPatterns(), 12 * 3600 + 55 * 60, DurationRounding::Floor, true), "12" NNBSP "год");
  // Under an hour there is nothing to drop.
  EXPECT_EQ(format(english().longPatterns(), 45 * 60, DurationRounding::Floor, true), "45 min");
}

TEST(DurationFormat, WidthGuardFallsBackToHours) {
  const auto p = ukrainian().longPatterns();
  const auto bytes = [](const char* text) { return static_cast<int>(std::strlen(text)); };
  char buf[48];

  formatDurationToFit(p, 12 * 3600 + 55 * 60, DurationRounding::Floor, 100, bytes, buf, sizeof(buf));
  EXPECT_STREQ(buf, "12" NNBSP "год 55" NNBSP "хв");

  const int fullWidth = bytes(buf);
  formatDurationToFit(p, 12 * 3600 + 55 * 60, DurationRounding::Floor, fullWidth - 1, bytes, buf, sizeof(buf));
  EXPECT_STREQ(buf, "12" NNBSP "год");

  formatDurationToFit(p, 12 * 3600 + 55 * 60, DurationRounding::Floor, 0, bytes, buf, sizeof(buf));
  EXPECT_STREQ(buf, "12" NNBSP "год 55" NNBSP "хв");
}

TEST(DurationFormat, WidthGuardKeepsEnglishHundredsOfHoursApart) {
  const auto p = english().longPatterns();
  const auto bytes = [](const char* text) { return static_cast<int>(std::strlen(text)); };
  char buf[48];
  formatDurationToFit(p, 123 * 3600 + 45 * 60, DurationRounding::Floor, 5, bytes, buf, sizeof(buf));
  EXPECT_STREQ(buf, "123h");
}

TEST(DurationFormat, SecondsEnglish) {
  const auto p = english().secondsPatterns();
  EXPECT_EQ(formatSeconds(p, 45), "45s");
  EXPECT_EQ(formatSeconds(p, 120), "2m");
  EXPECT_EQ(formatSeconds(p, 150), "2m 30s");
}

TEST(DurationFormat, SecondsUkrainian) {
  const auto p = ukrainian().secondsPatterns();
  EXPECT_EQ(formatSeconds(p, 45), "45" NNBSP "с");
  EXPECT_EQ(formatSeconds(p, 120), "2" NNBSP "хв");
  EXPECT_EQ(formatSeconds(p, 150), "2" NNBSP "хв 30" NNBSP "с");
}

TEST(DurationFormat, DecimalSeparator) {
  char buf[16] = "1.5";
  applyDecimalSeparator(buf, ",");
  EXPECT_STREQ(buf, "1,5");
  applyDecimalSeparator(buf, ".");
  EXPECT_STREQ(buf, "1,5");
  char multi[16] = "2.25";
  applyDecimalSeparator(multi, "\xD9\xAB");  // multi-byte separators are left alone
  EXPECT_STREQ(multi, "2.25");
  EXPECT_STREQ(ukrainian().get("STR_DECIMAL_SEPARATOR"), ",");
  EXPECT_STREQ(english().get("STR_DECIMAL_SEPARATOR"), ".");
}

TEST(DurationFormat, TranslationsKeepPlaceholders) {
  for (const char* key :
       {"STR_DURATION_MIN_FMT", "STR_DURATION_H_FMT", "STR_DURATION_H_MIN_FMT", "STR_DURATION_MIN_SHORT_FMT",
        "STR_DURATION_H_MIN_SHORT_FMT", "STR_DURATION_SEC_SHORT_FMT", "STR_DURATION_MIN_SEC_SHORT_FMT"}) {
    const std::string en = english().get(key);
    const std::string uk = ukrainian().get(key);
    const auto count = [](const std::string& s) {
      size_t n = 0;
      for (size_t pos = s.find("%lu"); pos != std::string::npos; pos = s.find("%lu", pos + 3)) ++n;
      return n;
    };
    EXPECT_EQ(count(en), count(uk)) << key;
    EXPECT_EQ(std::count(en.begin(), en.end(), '%'), std::count(uk.begin(), uk.end(), '%')) << key;
  }
}
