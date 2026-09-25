#pragma once

// Fork-only («Читанка» / Chytanka): the default sleep screen shows one of the
// quote cards embedded in flash. Only compiled in when the build defines
// CHYTANKA.

#include <string>

class GfxRenderer;

namespace chytanka {

// Picks the next card (shuffle bag persisted on the SD card), draws it with the
// same grayscale sequence SleepActivity uses for a Custom sleep BMP, and
// refreshes the panel. Returns false when the card cannot be drawn (decoder
// allocation failed, corrupt data, grayscale base refused); the caller then
// draws its own screen. Decode failures surface in the first (B/W) pass,
// before anything is sent to the panel.
//
// With a non-empty `readingTitle` the card also gets a small line above its
// brand footer, «Читаєте: <title> · 25%» ("Reading: ..." in any other UI
// language); `progressPercent` < 0 leaves the percentage out.
bool renderQuoteCardSleepScreen(GfxRenderer& renderer, bool turnOffScreen, const std::string& readingTitle = {},
                                float progressPercent = -1.0f);

}  // namespace chytanka
