#include <gtest/gtest.h>

#include <cstring>
#include <string>

#include "components/themes/ButtonHintLayout.h"

namespace {
constexpr int kPositions[] = {58, 146, 254, 342};  // X4 Lyra/Minimal hint boxes
constexpr int kWidth = 80;
constexpr ButtonHintBounds kScreen{0, 480};

constexpr int kBasePositions[] = {25, 130, 245, 350};  // X4 Classic: boxes 0 and 1 overlap by 1 px
constexpr int kBaseWidth = 106;

ButtonHintSpan fit(const int index, const int textWidth, const bool (&grows)[4]) {
  return fitButtonHintSpan(kPositions, grows, 4, index, kWidth, kScreen, textWidth);
}

// Fake font: every byte is 10 px wide.
int measure(const char* text) { return static_cast<int>(std::strlen(text)) * 10; }
}  // namespace

TEST(ButtonHintLayout, FittingLabelsKeepTheOriginalLayout) {
  const bool grows[] = {true, true, true, true};
  // Anything up to width - 1 fit before, including labels that touch the
  // border padding, so none of them may move or resize a box.
  for (int i = 0; i < 4; i++) {
    for (const int textWidth : {0, 40, kWidth - 2, kWidth - 1}) {
      const auto span = fit(i, textWidth, grows);
      EXPECT_EQ(span.x, kPositions[i]) << i << " " << textWidth;
      EXPECT_EQ(span.width, kWidth) << i << " " << textWidth;
      EXPECT_EQ(mirroredButtonHintX(kPositions, 4, i, span, kWidth), kPositions[3 - i]);
    }
  }
}

TEST(ButtonHintLayout, GrowsIntoSharedGapsByHalf) {
  // Every neighbour grows too, so each shared gap is split.
  const bool grows[] = {true, true, true, true};
  // Box 2 shares an 8 px gap on the left and a 28 px gap on the right.
  const auto span = fit(1, 200, grows);
  EXPECT_EQ(span.x, 146 - 3);
  EXPECT_EQ(span.x + span.width, 146 + kWidth + 13);
}

TEST(ButtonHintLayout, UsesWholeGapBesideNeighbourThatKeepsItsBox) {
  // Hint 3 is empty or fits its box, so hint 2 may take the whole gap.
  const bool grows[] = {true, true, false, true};
  const auto span = fit(1, 200, grows);
  EXPECT_EQ(span.x + span.width, 254 - 2);
}

TEST(ButtonHintLayout, ShiftsGrowthToTheRoomierSide) {
  const bool grows[] = {true, true, true, true};
  // Needs 4 + 3 px more; only 3 px fit into the gap shared with box 2.
  const auto span = fit(0, 83, grows);
  EXPECT_EQ(span.width, 87);
  EXPECT_EQ(span.x, 58 - 4);
  EXPECT_EQ(span.x + span.width, 58 + kWidth + 3);
}

TEST(ButtonHintLayout, NeighbouringBoxesNeverOverlap) {
  const bool grows[] = {true, true, true, true};
  for (int i = 0; i < 3; i++) {
    const auto left = fit(i, 300, grows);
    const auto right = fit(i + 1, 300, grows);
    EXPECT_LT(left.x + left.width, right.x) << i;
  }
  const auto last = fit(3, 300, grows);
  EXPECT_LE(last.x + last.width, kScreen.right);
  EXPECT_GE(fit(0, 300, grows).x, kScreen.left);
}

TEST(ButtonHintLayout, EdgeHintsStayInsideBezelSafeBounds) {
  const bool grows[] = {true, false, false, true};
  const ButtonHintBounds bezel{10, 470};
  const auto first = fitButtonHintSpan(kPositions, grows, 4, 0, kWidth, bezel, 300);
  const auto last = fitButtonHintSpan(kPositions, grows, 4, 3, kWidth, bezel, 300);
  EXPECT_EQ(first.x, 10);
  EXPECT_EQ(last.x + last.width, 470);
}

TEST(ButtonHintLayout, OverlappingClassicBoxesDoNotGrowIntoEachOther) {
  // Classic's first two X4 boxes overlap (25 + 106 > 130): no room between them.
  const bool grows[] = {true, true, true, true};
  const auto first = fitButtonHintSpan(kBasePositions, grows, 4, 0, kBaseWidth, kScreen, 200);
  const auto second = fitButtonHintSpan(kBasePositions, grows, 4, 1, kBaseWidth, kScreen, 200);
  EXPECT_EQ(first.x + first.width, 25 + kBaseWidth);
  EXPECT_EQ(second.x, 130);
  EXPECT_EQ(second.x + second.width, 130 + kBaseWidth + 3);  // half of the 9 px gap, minus the gap floor
}

TEST(ButtonHintLayout, InvertedTextMirrorsTheGrowth) {
  const bool grows[] = {true, true, true, true};
  const auto span = fit(1, 200, grows);  // grows 3 px left, 13 px right
  // Drawn upside down, hint 1 sits in slot 2 and its right growth is on the left.
  EXPECT_EQ(mirroredButtonHintX(kPositions, 4, 1, span, kWidth), 254 - 13);
}

TEST(ButtonHintLayout, RowTruncatesOnlyGrowingLabels) {
  const char* labels[] = {"<< Back", "A much too long label", nullptr, ""};
  int truncations = 0;
  ButtonHintRow row;
  layoutButtonHints(row, labels, kPositions, kWidth, kScreen, measure, [&](const char* text, const int maxWidth) {
    ++truncations;
    return std::string(text, static_cast<size_t>(maxWidth / 10));
  });
  EXPECT_EQ(truncations, 1);
  EXPECT_FALSE(row.grows[0]);
  EXPECT_EQ(row.text(0), labels[0]);  // drawn directly, no copy
  EXPECT_EQ(row.textWidths[0], 70);
  EXPECT_TRUE(row.grows[1]);
  EXPECT_LE(row.textWidths[1], row.spans[1].width - kButtonHintTextPadding * 2);
  EXPECT_FALSE(row.labelled[2]);
  EXPECT_FALSE(row.labelled[3]);
}

namespace {
ButtonHintRow layoutRow(const char* second) {
  const char* labels[] = {"<< Back", second, "Up", "Down"};
  ButtonHintRow row;
  layoutButtonHints(row, labels, kPositions, kWidth, kScreen, measure,
                    [](const char* text, const int maxWidth) { return std::string(text, maxWidth / 10); });
  return row;
}

int spansToClear(const ButtonHintRow& row, ButtonHintHistory& history, ButtonHintSpan (&out)[4]) {
  return buttonHintSpansToClear(row, kPositions, kWidth, history, out);
}
}  // namespace

TEST(ButtonHintLayout, FittingRedrawsClearNothingExtra) {
  ButtonHintHistory history;
  ButtonHintSpan out[4];
  EXPECT_EQ(spansToClear(layoutRow("Open"), history, out), 0);    // first draw
  EXPECT_EQ(spansToClear(layoutRow("Select"), history, out), 0);  // fit -> fit
}

TEST(ButtonHintLayout, ShrinkingBackClearsTheWiderBoxOnce) {
  ButtonHintHistory history;
  ButtonHintSpan out[4];
  const ButtonHintRow grown = layoutRow("A much too long label");
  ASSERT_TRUE(grown.grows[1]);
  EXPECT_EQ(spansToClear(grown, history, out), 0);              // fit -> grow: the box paints itself
  ASSERT_EQ(spansToClear(layoutRow("Open"), history, out), 1);  // grow -> fit
  EXPECT_EQ(out[0].x, grown.spans[1].x);
  EXPECT_EQ(out[0].width, grown.spans[1].width);
  EXPECT_EQ(spansToClear(layoutRow("Open"), history, out), 0);  // only once
}

TEST(ButtonHintLayout, UnchangedGrownBoxIsNotClearedAgain) {
  ButtonHintHistory history;
  ButtonHintSpan out[4];
  spansToClear(layoutRow("A much too long label"), history, out);
  EXPECT_EQ(spansToClear(layoutRow("A much too long label"), history, out), 0);
}

TEST(ButtonHintLayout, UntouchedSlotKeepsItsRecord) {
  ButtonHintHistory history;
  ButtonHintSpan out[4];
  spansToClear(layoutRow("A much too long label"), history, out);
  const char* untouched[] = {nullptr, nullptr, nullptr, nullptr};
  ButtonHintRow row;
  layoutButtonHints(row, untouched, kPositions, kWidth, kScreen, measure,
                    [](const char* text, const int) { return std::string(text); });
  EXPECT_EQ(spansToClear(row, history, out), 0);
  EXPECT_EQ(spansToClear(layoutRow("Open"), history, out), 1);
}
