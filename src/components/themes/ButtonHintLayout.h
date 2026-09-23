#pragma once

#include <algorithm>
#include <string>

// Front button hints sit above fixed physical buttons, so a label that is
// wider than its box (common outside English) used to spill over the border.
// The box grows into the free space beside it instead: half of a gap it
// shares with a neighbour that grows too, the whole gap otherwise (a fitting
// or empty neighbour keeps its own box), up to the viewable screen edge.
// Callers truncate whatever still does not fit.
struct ButtonHintSpan {
  int x;
  int width;
};

// Horizontal range a hint box may grow into (bezel-safe screen edges).
struct ButtonHintBounds {
  int left;
  int right;
};

// Horizontal padding kept between a widened hint label and its box border.
constexpr int kButtonHintTextPadding = 2;

// The themes centre a label with (width - 1 - textWidth) / 2, so it fits while
// it is at most width - 1 pixels wide.
inline bool buttonHintFits(const int textWidth, const int baseWidth) { return textWidth <= baseWidth - 1; }

// grows[i] is true for a labelled hint whose text does not fit its box.
inline ButtonHintSpan fitButtonHintSpan(const int* positions, const bool* grows, const int count, const int index,
                                        const int baseWidth, const ButtonHintBounds bounds, const int textWidth) {
  const int x = positions[index];
  // A label that already fits keeps the original box, so layouts that fit are
  // drawn exactly as before.
  if (buttonHintFits(textWidth, baseWidth)) return {x, baseWidth};
  const long needed = static_cast<long>(textWidth) + kButtonHintTextPadding * 2;

  constexpr int kMinGap = 2;
  const auto room = [](const int gap, const bool neighbourGrows) {
    return std::max(0, neighbourGrows ? (gap - kMinGap) / 2 : gap - kMinGap);
  };
  const int right = x + baseWidth;
  const int leftRoom =
      index == 0 ? std::max(0, x - bounds.left) : room(x - (positions[index - 1] + baseWidth), grows[index - 1]);
  const int rightRoom =
      index == count - 1 ? std::max(0, bounds.right - right) : room(positions[index + 1] - right, grows[index + 1]);
  const int extra = static_cast<int>(std::min<long>(needed - baseWidth, leftRoom + rightRoom));
  int growLeft = std::min(extra / 2, leftRoom);
  const int growRight = std::min(extra - growLeft, rightRoom);
  growLeft = std::min(extra - growRight, leftRoom);
  return {x - growLeft, baseWidth + growLeft + growRight};
}

// Inverted text is drawn in the rotated orientation, where hint i sits at the
// slot of hint count-1-i. A widened box keeps that slot and mirrors its growth:
// what grew to the right in portrait grows to the left here.
inline int mirroredButtonHintX(const int* positions, const int count, const int index, const ButtonHintSpan& span,
                               const int baseWidth) {
  const int rightGrowth = span.x + span.width - (positions[index] + baseWidth);
  return positions[count - 1 - index] - rightGrowth;
}

// Layout of one row of four front button hints, shared by the themes.
struct ButtonHintRow {
  static constexpr int kCount = 4;
  const char* labels[kCount] = {};
  bool labelled[kCount] = {};
  bool grows[kCount] = {};
  // Width of the text as drawn (after truncation for a grown hint).
  int textWidths[kCount] = {};
  ButtonHintSpan spans[kCount] = {};
  ButtonHintBounds bounds{};
  // Only grown hints are truncated; fitting ones draw labels[i] directly.
  std::string truncated[kCount];

  const char* text(const int i) const { return grows[i] ? truncated[i].c_str() : labels[i]; }
};

// measure(const char*) -> int pixel width; truncate(const char*, int maxWidth)
// -> std::string with an ellipsis. Labels are measured once; only a label that
// grows its box is truncated (the one case that allocates).
template <typename Measure, typename Truncate>
void layoutButtonHints(ButtonHintRow& row, const char* const* labels, const int* positions, const int baseWidth,
                       const ButtonHintBounds bounds, Measure&& measure, Truncate&& truncate) {
  constexpr int n = ButtonHintRow::kCount;
  for (int i = 0; i < n; i++) {
    row.labels[i] = labels[i];
    row.labelled[i] = labels[i] != nullptr && labels[i][0] != '\0';
    row.textWidths[i] = row.labelled[i] ? measure(labels[i]) : 0;
    row.grows[i] = row.labelled[i] && !buttonHintFits(row.textWidths[i], baseWidth);
  }
  for (int i = 0; i < n; i++) {
    if (!row.labelled[i]) continue;
    row.spans[i] = fitButtonHintSpan(positions, row.grows, n, i, baseWidth, bounds, row.textWidths[i]);
    if (row.grows[i]) {
      row.truncated[i] = truncate(labels[i], row.spans[i].width - kButtonHintTextPadding * 2);
      row.textWidths[i] = measure(row.truncated[i].c_str());
    }
  }
}

// What each hint slot looked like on the previous draw. Only a box that grew
// last time needs clearing now: a fitting layout paints exactly its own boxes,
// as before hints could grow, so nothing drawn beside them (an image, page
// text) gets overpainted.
struct ButtonHintHistory {
  ButtonHintSpan spans[ButtonHintRow::kCount] = {};
  bool known[ButtonHintRow::kCount] = {};
};

// Fills `out` with the previous spans to clear before drawing `row` and records
// this draw in `history`. Returns how many spans to clear. Slots passed as
// nullptr are left untouched on screen, so their record is kept.
inline int buttonHintSpansToClear(const ButtonHintRow& row, const int* positions, const int baseWidth,
                                  ButtonHintHistory& history, ButtonHintSpan (&out)[ButtonHintRow::kCount]) {
  int count = 0;
  for (int i = 0; i < ButtonHintRow::kCount; i++) {
    if (row.labels[i] == nullptr) continue;
    const ButtonHintSpan base{positions[i], baseWidth};
    const ButtonHintSpan now = row.labelled[i] ? row.spans[i] : base;
    if (history.known[i]) {
      const ButtonHintSpan& before = history.spans[i];
      const bool grewBefore = before.width > baseWidth;
      const bool sameSpan = before.x == now.x && before.width == now.width;
      if (grewBefore && !sameSpan) out[count++] = before;
    }
    history.spans[i] = now;
    history.known[i] = true;
  }
  return count;
}
