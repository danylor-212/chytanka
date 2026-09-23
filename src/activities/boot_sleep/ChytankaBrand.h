#pragma once

// Fork-only («Читанка» / Chytanka): brand block for the default boot and sleep
// screens. Only compiled in when the build defines CHYTANKA.

class GfxRenderer;

namespace chytanka {

// Draws the centred Chytanka logo, the brand name, the firmware version, a
// small "based on CrossInk" credit line and `status` below them. Replaces the CrossInk logo
// block; the caller clears the screen and refreshes the display.
void drawBrandBlock(const GfxRenderer& renderer, int pageWidth, int pageHeight, const char* status);

}  // namespace chytanka
