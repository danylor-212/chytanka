#include <gtest/gtest.h>

#include <string>

#include "ChytankaReadingLine.h"
#include "ChytankaWelcome.h"

using chytanka::buildReadingLine;
using chytanka::decideWelcome;
using chytanka::readingLinePercent;
using chytanka::WelcomeDecision;
using chytanka::WelcomeState;

namespace {

WelcomeState crossInkUpgrade() {
  WelcomeState s;
  s.settingsFileExists = true;
  return s;
}

// Byte length as a stand-in for rendered width.
auto fitsBytes(size_t max) {
  return [max](const std::string& s) { return s.size() <= max; };
}

bool validUtf8(const std::string& s) {
  for (size_t i = 0; i < s.size();) {
    const auto c = static_cast<unsigned char>(s[i]);
    const size_t n = c < 0x80 ? 1 : (c >> 5) == 0x6 ? 2 : (c >> 4) == 0xE ? 3 : (c >> 3) == 0x1E ? 4 : 0;
    if (n == 0 || i + n > s.size()) return false;
    for (size_t k = 1; k < n; k++) {
      if ((static_cast<unsigned char>(s[i + k]) & 0xC0) != 0x80) return false;
    }
    i += n;
  }
  return true;
}

}  // namespace

TEST(ChytankaWelcome, MarkerMeansNothingToDo) {
  WelcomeState s = crossInkUpgrade();
  s.markerExists = true;
  EXPECT_EQ(decideWelcome(s), WelcomeDecision::None);
}

TEST(ChytankaWelcome, FreshCardIsMarkedSilently) {
  WelcomeState s;
  EXPECT_EQ(decideWelcome(s), WelcomeDecision::MarkOnly);
}

TEST(ChytankaWelcome, EarlierChytankaIsMarkedSilently) {
  WelcomeState s = crossInkUpgrade();
  s.usedChytankaBefore = true;
  EXPECT_EQ(decideWelcome(s), WelcomeDecision::MarkOnly);
}

TEST(ChytankaWelcome, NothingToOfferIsMarkedSilently) {
  WelcomeState s = crossInkUpgrade();
  s.languageIsUkrainian = true;
  s.readingIsRecommended = true;
  EXPECT_EQ(decideWelcome(s), WelcomeDecision::MarkOnly);
}

TEST(ChytankaWelcome, CrossInkUpgradeWithSomethingToOfferShows) {
  WelcomeState s = crossInkUpgrade();
  EXPECT_EQ(decideWelcome(s), WelcomeDecision::Show);
  s.languageIsUkrainian = true;
  EXPECT_EQ(decideWelcome(s), WelcomeDecision::Show);
  s.languageIsUkrainian = false;
  s.readingIsRecommended = true;
  EXPECT_EQ(decideWelcome(s), WelcomeDecision::Show);
}

TEST(ChytankaReadingLine, Percent) {
  EXPECT_EQ(readingLinePercent(-1.0f), -1);
  EXPECT_EQ(readingLinePercent(0.0f), 0);
  EXPECT_EQ(readingLinePercent(25.7f), 25);
  EXPECT_EQ(readingLinePercent(99.99f), 99);
  EXPECT_EQ(readingLinePercent(100.0f), 100);
  EXPECT_EQ(readingLinePercent(140.0f), 100);
  EXPECT_EQ(readingLinePercent(0.0f / 0.0f), -1);
}

TEST(ChytankaReadingLine, FullLineInBothLanguages) {
  EXPECT_EQ(buildReadingLine("Я (Романтика)", 25, true, fitsBytes(200)), "Читаєте: Я (Романтика) · 25%");
  EXPECT_EQ(buildReadingLine("Emma", 7, false, fitsBytes(200)), "Reading: Emma · 7%");
  EXPECT_EQ(buildReadingLine("Emma", -1, false, fitsBytes(200)), "Reading: Emma");
}

TEST(ChytankaReadingLine, NoTitleNoLine) { EXPECT_EQ(buildReadingLine("", 25, true, fitsBytes(200)), ""); }

TEST(ChytankaReadingLine, TruncatesTitleOnCodepointBoundaries) {
  const std::string title = "Незавершений рукопис";
  for (size_t max = 0; max < 80; max++) {
    const std::string line = buildReadingLine(title, 42, true, fitsBytes(max));
    if (line.empty()) continue;
    EXPECT_LE(line.size(), max);
    EXPECT_TRUE(validUtf8(line)) << line;
    EXPECT_EQ(line.rfind("Читаєте: ", 0), 0u) << line;
    EXPECT_NE(line.find(" · 42%"), std::string::npos) << line;
    if (line.find(title) == std::string::npos) {
      EXPECT_NE(line.find("…"), std::string::npos) << line;
      EXPECT_EQ(line.find(" …"), std::string::npos) << "no space before the ellipsis: " << line;
    }
  }
  EXPECT_EQ(buildReadingLine(title, 42, true, fitsBytes(5)), "");
}
