#pragma once

// Fork-only («Читанка» / Chytanka): the default sleep screen shows one of the
// quote cards embedded in flash. Only compiled in when the build defines
// CHYTANKA.

class GfxRenderer;

namespace chytanka {

// Picks the next card (shuffle bag persisted on the SD card), draws it with the
// same grayscale sequence SleepActivity uses for a Custom sleep BMP, and
// refreshes the panel. Returns false when the card cannot be drawn (decoder
// allocation failed, corrupt data, grayscale base refused); the caller then
// draws its own screen. Decode failures surface in the first (B/W) pass,
// before anything is sent to the panel.
bool renderQuoteCardSleepScreen(GfxRenderer& renderer, bool turnOffScreen);

}  // namespace chytanka
