#ifdef CHYTANKA

#include "ChytankaQuoteDecoder.h"

#include <cstring>

namespace chytanka {

static_assert(QUOTE_CARD_ROW_BYTES * 4 == QUOTE_CARD_WIDTH, "2bpp rows without padding");
static_assert(QUOTE_CARD_COUNT >= 1 && QUOTE_CARD_COUNT <= 64, "pickQuoteCard uses a 64-bit mask");

void QuoteCardDecoder::begin(const uint8_t* data, const size_t size, const uint32_t expectedCrc) {
  memset(&inflater, 0, sizeof(inflater));
  // Zeroed ring: a (corrupt) back-reference before the first output byte reads
  // zeros instead of a previous card, and the CRC still rejects it.
  memset(window, 0, sizeof(window));
  uzlib_uncompress_init(&inflater, window, sizeof(window));
  inflater.source = data;
  inflater.source_limit = data + size;
  inflater.source_read_cb = nullptr;
  this->expectedCrc = expectedCrc;
  crc = 0;
  rowsRead = 0;
  failed = data == nullptr;
}

bool QuoteCardDecoder::beginCard(const int index) {
  if (index < 0 || index >= QUOTE_CARD_COUNT) {
    failed = true;
    return false;
  }
  const QuoteCardEntry& entry = QUOTE_CARDS[index];
  begin(QUOTE_CARD_DATA + entry.offset, entry.size, entry.crc32);
  return true;
}

const uint8_t* QuoteCardDecoder::nextRow() {
  if (failed || rowsRead >= QUOTE_CARD_HEIGHT) return nullptr;

  inflater.dest_start = row;
  inflater.dest = row;
  inflater.dest_limit = row + sizeof(row);
  const int res = uzlib_uncompress(&inflater);
  // uzlib returns TINF_OK as soon as the row is full, before it reads the
  // end-of-stream marker, so a valid row never comes with TINF_DONE: DONE here
  // means the stream ended early (truncated). finish() checks the real end.
  if (res != TINF_OK || inflater.dest != inflater.dest_limit) {
    failed = true;
    return nullptr;
  }
  crc = uzlib_crc32(row, sizeof(row), crc);
  rowsRead++;
  return row;
}

bool QuoteCardDecoder::finish() {
  if (failed || rowsRead != QUOTE_CARD_HEIGHT) return false;

  // The stream must end right after the last row: one more inflate step has
  // to report TINF_DONE without producing output.
  uint8_t extra = 0;
  inflater.dest_start = &extra;
  inflater.dest = &extra;
  inflater.dest_limit = &extra + 1;
  const int res = uzlib_uncompress(&inflater);
  if (res != TINF_DONE || inflater.dest != &extra) {
    failed = true;
    return false;
  }
  return crc == expectedCrc;
}

int pickQuoteCard(QuoteCardHistory& history, const int count, uint32_t (*randomBelow)(uint32_t n)) {
  if (count < 1 || count > 64 || randomBelow == nullptr) return -1;

  const uint64_t all = count == 64 ? ~uint64_t{0} : (uint64_t{1} << count) - 1;
  if (history.count != count || (history.shown & ~all) != 0 || (history.last != 0xFF && history.last >= count)) {
    history = QuoteCardHistory{};
    history.count = static_cast<uint8_t>(count);
  }

  const auto eligible = [&](const uint64_t shown) {
    uint64_t mask = all & ~shown;
    if (count > 1 && history.last < count) mask &= ~(uint64_t{1} << history.last);
    return mask;
  };

  uint64_t candidates = eligible(history.shown);
  if (candidates == 0) {
    history.shown = 0;  // cycle complete: start a new bag
    candidates = eligible(0);
  }

  uint32_t skip = randomBelow(static_cast<uint32_t>(__builtin_popcountll(candidates)));
  int index = 0;
  for (; index < count; index++) {
    if ((candidates >> index) & 1) {
      if (skip == 0) break;
      skip--;
    }
  }
  if (index >= count) index = __builtin_ctzll(candidates);  // randomBelow out of range

  history.shown |= uint64_t{1} << index;
  history.last = static_cast<uint8_t>(index);
  return index;
}

}  // namespace chytanka

#endif  // CHYTANKA
