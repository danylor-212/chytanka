#pragma once

#include <GfxRenderer.h>

#include "components/themes/ButtonHintLayout.h"

// Lays out a row of front button hints drawn in `fontId`, growing boxes no
// further than the bezel-safe viewable edges of the current orientation.
inline void layoutButtonHintRow(const GfxRenderer& renderer, const int fontId, ButtonHintRow& row,
                                const char* const* labels, const int* positions, const int baseWidth) {
  int top = 0;
  int right = 0;
  int bottom = 0;
  int left = 0;
  renderer.getOrientedViewableTRBL(&top, &right, &bottom, &left);
  row.bounds = ButtonHintBounds{left, renderer.getScreenWidth() - right};
  layoutButtonHints(
      row, labels, positions, baseWidth, row.bounds,
      [&renderer, fontId](const char* text) { return renderer.getTextWidth(fontId, text); },
      [&renderer, fontId](const char* text, const int maxWidth) {
        return renderer.truncatedText(fontId, text, maxWidth);
      });
}
