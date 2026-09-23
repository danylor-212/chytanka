#pragma once

// Fork-only («Читанка» / Chytanka): streaming decoder and shuffle-bag picker
// for the quote cards embedded in flash (src/images/ChytankaQuoteCards.*).
// Hardware-free so the native test suite can link it; only compiled in when
// the build defines CHYTANKA.

#include <uzlib.h>

#include <cstddef>
#include <cstdint>

#include "images/ChytankaQuoteCards.h"

namespace chytanka {

// Inflates one card row by row. Everything lives inside the object (~1.9 KB):
// uzlib state (~1.3 KB), the 512-byte back-reference ring the generator's
// deflate window allows, and one 120-byte output row. The decompressed card
// (96 KB) never exists in RAM at once. Heap-allocate one instance for the
// duration of a render; it holds no other resources.
class QuoteCardDecoder {
 public:
  // Starts (or restarts) decoding a raw deflate stream of QUOTE_CARD_HEIGHT
  // rows whose CRC-32 must equal `expectedCrc`. `data` must outlive decoding.
  void begin(const uint8_t* data, size_t size, uint32_t expectedCrc);
  // Starts QUOTE_CARDS[index]. Returns false for an out-of-range index.
  bool beginCard(int index);

  // Next QUOTE_CARD_ROW_BYTES of 2bpp pixels (0 black .. 3 white, leftmost
  // pixel in the high bits), or nullptr on a corrupt/truncated stream or once
  // all rows were returned. The buffer is reused by the next call.
  const uint8_t* nextRow();

  // True when every row was read, the stream ended exactly there and the CRC
  // matched. Call after the last nextRow().
  bool finish();

 private:
  uzlib_uncomp inflater = {};
  uint8_t window[QUOTE_CARD_WINDOW_SIZE] = {};
  uint8_t row[QUOTE_CARD_ROW_BYTES] = {};
  uint32_t expectedCrc = 0;
  uint32_t crc = 0;
  int rowsRead = 0;
  bool failed = true;
};

// Gray level to draw for a decoded card pixel (`level` 0 black .. 3 white).
// Dark sleep mode shows the card inverted (light text on black) by mapping
// levels before every render pass: 0<->3, 1<->2. Inverting the finished B/W
// framebuffer instead would leave the gray planes un-inverted.
constexpr uint8_t quoteCardLevel(const uint8_t level, const bool inverted) {
  return inverted ? static_cast<uint8_t>(3 - level) : level;
}

// Level of the card background as drawn: white, or black when inverted. It is
// always a source-white (3) pixel, i.e. a 0xFF byte covers four of them.
constexpr uint8_t quoteCardBackground(const bool inverted) { return quoteCardLevel(3, inverted); }

// Framebuffer byte to clear to before a pass so that every background pixel
// already has its final value and can be skipped (drawPixel(true) clears a
// bit: 0x00 = all black, 0xFF = all white):
//   B/W pass:            background white -> 0xFF, black -> 0x00.
//   Absolute gray plane: grayPlanePixel() writes level 3 as white and level 0
//                        as black in both planes -> 0xFF / 0x00 likewise.
//   Relative gray plane: only levels 1 and 2 are ever written, background
//                        (0 or 3) never is -> start empty (0x00), as upstream.
constexpr uint8_t quoteCardClearByte(const bool grayPass, const bool absolute, const bool inverted) {
  if (grayPass && !absolute) return 0x00;
  return inverted ? 0x00 : 0xFF;
}

// True when a decoded row is all background (every source pixel white).
inline bool quoteCardRowIsBackground(const uint8_t* row) {
  for (int i = 0; i < QUOTE_CARD_ROW_BYTES; i++) {
    if (row[i] != 0xFF) return false;
  }
  return true;
}

// Calls plot(cx, level) for each pixel cx in [firstX, endX) of a decoded row
// whose drawn level differs from the background; background pixels (and whole
// 0xFF bytes) are skipped, relying on the pass having been cleared to
// quoteCardClearByte().
template <typename Plot>
void forEachQuoteCardForegroundPixel(const uint8_t* row, const int firstX, const int endX, const bool inverted,
                                     Plot&& plot) {
  for (int cx = firstX; cx < endX;) {
    const uint8_t packed = row[cx >> 2];
    if (packed == 0xFF) {
      cx = (cx | 3) + 1;  // next byte: four background pixels
      continue;
    }
    const uint8_t level = (packed >> (6 - ((cx & 3) << 1))) & 0x3;
    if (level != 3) plot(cx, quoteCardLevel(level, inverted));
    cx++;
  }
}

// Persisted shuffle-bag state: every card is shown once per cycle, in random
// order, and a new cycle never starts with the card that ended the last one.
struct QuoteCardHistory {
  uint64_t shown = 0;   // bit i set: card i already shown this cycle
  uint8_t count = 0;    // QUOTE_CARD_COUNT the mask was built for
  uint8_t last = 0xFF;  // most recently shown card, 0xFF = none
};

// Picks the next card in [0, count) (count 1..64) and records it in `history`.
// Inconsistent history (another card count, stray bits) restarts the bag.
// `randomBelow(n)` must return a uniform value in [0, n). Returns -1 only for
// an invalid count.
int pickQuoteCard(QuoteCardHistory& history, int count, uint32_t (*randomBelow)(uint32_t n));

}  // namespace chytanka
