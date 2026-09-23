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
