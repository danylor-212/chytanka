#include <gtest/gtest.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include "BitmapHelpers.h"  // grayPlanePixel()
#include "ChytankaQuoteDecoder.h"

using chytanka::forEachQuoteCardForegroundPixel;
using chytanka::pickQuoteCard;
using chytanka::QUOTE_CARD_COUNT;
using chytanka::QUOTE_CARD_DATA;
using chytanka::QUOTE_CARD_HEIGHT;
using chytanka::QUOTE_CARD_ROW_BYTES;
using chytanka::QUOTE_CARD_WIDTH;
using chytanka::QUOTE_CARDS;
using chytanka::quoteCardClearByte;
using chytanka::QuoteCardDecoder;
using chytanka::QuoteCardHistory;
using chytanka::quoteCardLevel;
using chytanka::quoteCardRowIsBackground;

namespace {

// Decodes a whole card into one level (0..3) per pixel. Returns false on any
// decoder failure, including the final CRC / end-of-stream check.
bool decodeLevels(QuoteCardDecoder& decoder, std::vector<uint8_t>& levels) {
  levels.assign(static_cast<size_t>(QUOTE_CARD_WIDTH) * QUOTE_CARD_HEIGHT, 0xFF);
  for (int y = 0; y < QUOTE_CARD_HEIGHT; y++) {
    const uint8_t* row = decoder.nextRow();
    if (!row) return false;
    for (int x = 0; x < QUOTE_CARD_WIDTH; x++) {
      levels[static_cast<size_t>(y) * QUOTE_CARD_WIDTH + x] = (row[x >> 2] >> (6 - ((x & 3) << 1))) & 0x3;
    }
  }
  return decoder.finish();
}

// Minimal reader for the brand cards: uncompressed 8-bit paletted BMP.
bool readBmpLevels(const std::string& path, std::vector<uint8_t>& levels) {
  std::ifstream in(path, std::ios::binary);
  if (!in) return false;
  const std::vector<uint8_t> f((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  if (f.size() < 54 || f[0] != 'B' || f[1] != 'M') return false;
  const auto le32 = [&](size_t o) {
    return static_cast<uint32_t>(f[o] | (f[o + 1] << 8) | (f[o + 2] << 16) | (static_cast<uint32_t>(f[o + 3]) << 24));
  };
  const uint32_t dataOffset = le32(10);
  const uint32_t dibSize = le32(14);
  const int32_t width = static_cast<int32_t>(le32(18));
  const int32_t height = static_cast<int32_t>(le32(22));
  const uint16_t bpp = static_cast<uint16_t>(f[28] | (f[29] << 8));
  if (width != QUOTE_CARD_WIDTH || std::abs(height) != QUOTE_CARD_HEIGHT || bpp != 8 || le32(30) != 0) return false;
  uint32_t colors = le32(46);
  if (colors == 0) colors = 256;
  const size_t palette = 14 + dibSize;
  const size_t stride = (static_cast<size_t>(width) + 3) & ~size_t{3};
  if (dataOffset + stride * QUOTE_CARD_HEIGHT > f.size()) return false;

  levels.assign(static_cast<size_t>(QUOTE_CARD_WIDTH) * QUOTE_CARD_HEIGHT, 0xFF);
  for (int y = 0; y < QUOTE_CARD_HEIGHT; y++) {
    const int srcY = height > 0 ? QUOTE_CARD_HEIGHT - 1 - y : y;
    const uint8_t* src = f.data() + dataOffset + stride * srcY;
    for (int x = 0; x < QUOTE_CARD_WIDTH; x++) {
      if (src[x] >= colors) return false;
      const uint8_t gray = f[palette + src[x] * 4];  // blue == green == red
      levels[static_cast<size_t>(y) * QUOTE_CARD_WIDTH + x] = gray >> 6;
    }
  }
  return true;
}

uint32_t sequence[4096];
size_t sequencePos = 0;
uint32_t scriptedRandom(const uint32_t n) { return sequence[sequencePos++ % 4096] % n; }
uint32_t firstRandom(uint32_t) { return 0; }
uint32_t lastRandom(const uint32_t n) { return n - 1; }
uint32_t outOfRangeRandom(const uint32_t n) { return n + 7; }

}  // namespace

TEST(ChytankaQuoteCards, TableIsContiguousAndInBounds) {
  uint32_t expectedOffset = 0;
  for (int i = 0; i < QUOTE_CARD_COUNT; i++) {
    EXPECT_EQ(QUOTE_CARDS[i].offset, expectedOffset) << "card " << i;
    EXPECT_GT(QUOTE_CARDS[i].size, 0u);
    expectedOffset += QUOTE_CARDS[i].size;
  }
  EXPECT_EQ(expectedOffset, chytanka::QUOTE_CARD_DATA_SIZE);
}

// The CRC in the table is computed by the generator from the source BMP's
// pixel levels, so a match means every embedded card decodes to its source.
TEST(ChytankaQuoteCards, EveryCardDecodesToItsSourceCrc) {
  QuoteCardDecoder decoder;
  std::vector<uint8_t> levels;
  for (int i = 0; i < QUOTE_CARD_COUNT; i++) {
    ASSERT_TRUE(decoder.beginCard(i));
    EXPECT_TRUE(decodeLevels(decoder, levels)) << "card " << i << " (id " << QUOTE_CARDS[i].quoteId << ")";
    EXPECT_EQ(decoder.nextRow(), nullptr) << "rows past the card end";
  }
}

TEST(ChytankaQuoteCards, CardsUseGrayLevels) {
  QuoteCardDecoder decoder;
  std::vector<uint8_t> levels;
  ASSERT_TRUE(decoder.beginCard(0));
  ASSERT_TRUE(decodeLevels(decoder, levels));
  int histogram[4] = {};
  for (const uint8_t level : levels) histogram[level]++;
  EXPECT_GT(histogram[0], 0);                       // black text
  EXPECT_GT(histogram[1] + histogram[2], 0);        // anti-aliasing / ornament grays
  EXPECT_GT(histogram[3], QUOTE_CARD_WIDTH * 400);  // mostly white card
}

// The X3 (528x792 portrait) shows the card centred and unscaled, clipping rows
// 0-3 and 796-799: every card must keep them background (white).
TEST(ChytankaQuoteCards, EveryCardKeepsX3ClippedRowsBlank) {
  constexpr int clipped = (QUOTE_CARD_HEIGHT - 792) / 2;
  static_assert(clipped == 4);
  QuoteCardDecoder decoder;
  std::vector<uint8_t> levels;
  for (int i = 0; i < QUOTE_CARD_COUNT; i++) {
    ASSERT_TRUE(decoder.beginCard(i));
    ASSERT_TRUE(decodeLevels(decoder, levels));
    for (int y = 0; y < QUOTE_CARD_HEIGHT; y++) {
      if (y >= clipped && y < QUOTE_CARD_HEIGHT - clipped) continue;
      for (int x = 0; x < QUOTE_CARD_WIDTH; x++) {
        ASSERT_EQ(levels[y * QUOTE_CARD_WIDTH + x], 3)
            << "card " << i << " (id " << QUOTE_CARDS[i].quoteId << ") at " << x << "," << y;
      }
    }
  }
}

TEST(ChytankaQuoteCards, RestartDecodesIdentically) {
  QuoteCardDecoder decoder;
  std::vector<uint8_t> first, second;
  ASSERT_TRUE(decoder.beginCard(7));
  for (int y = 0; y < 100; y++) ASSERT_NE(decoder.nextRow(), nullptr);  // abandon mid-card
  ASSERT_TRUE(decoder.beginCard(7));
  ASSERT_TRUE(decodeLevels(decoder, first));
  ASSERT_TRUE(decoder.beginCard(7));
  ASSERT_TRUE(decodeLevels(decoder, second));
  EXPECT_EQ(first, second);
}

TEST(ChytankaQuoteCards, MatchesSourceBmpWhenBrandDirIsSet) {
  const char* brand = std::getenv("CHYTANKA_BRAND_DIR");
  if (!brand || !*brand) GTEST_SKIP() << "CHYTANKA_BRAND_DIR not set";

  QuoteCardDecoder decoder;
  std::vector<uint8_t> decoded, source;
  for (int i = 0; i < QUOTE_CARD_COUNT; i++) {
    char path[512];
    snprintf(path, sizeof(path), "%s/cards/public/q%03u.bmp", brand, QUOTE_CARDS[i].quoteId);
    ASSERT_TRUE(readBmpLevels(path, source)) << path;
    ASSERT_TRUE(decoder.beginCard(i));
    ASSERT_TRUE(decodeLevels(decoder, decoded));
    EXPECT_EQ(decoded, source) << path;
  }
}

// Writes each decoded card (Light and Dark) as a PGM for visual checks.
TEST(ChytankaQuoteCards, DumpPgmWhenRequested) {
  const char* dir = std::getenv("CHYTANKA_QUOTE_DUMP_DIR");
  if (!dir || !*dir) GTEST_SKIP() << "CHYTANKA_QUOTE_DUMP_DIR not set";
  QuoteCardDecoder decoder;
  std::vector<uint8_t> levels;
  for (int i = 0; i < QUOTE_CARD_COUNT; i++) {
    ASSERT_TRUE(decoder.beginCard(i));
    ASSERT_TRUE(decodeLevels(decoder, levels));
    char path[512];
    for (const bool dark : {false, true}) {
      snprintf(path, sizeof(path), "%s/card%02d_q%03u%s.pgm", dir, i, QUOTE_CARDS[i].quoteId, dark ? "_dark" : "");
      std::ofstream out(path, std::ios::binary);
      out << "P5\n" << QUOTE_CARD_WIDTH << " " << QUOTE_CARD_HEIGHT << "\n255\n";
      for (const uint8_t level : levels) out.put(static_cast<char>(quoteCardLevel(level, dark) * 85));
      ASSERT_TRUE(out.good()) << path;
    }
  }
}

TEST(ChytankaQuoteCards, CorruptOrTruncatedDataIsRejected) {
  const auto& entry = QUOTE_CARDS[3];
  std::vector<uint8_t> data(QUOTE_CARD_DATA + entry.offset, QUOTE_CARD_DATA + entry.offset + entry.size);
  QuoteCardDecoder decoder;
  std::vector<uint8_t> levels;

  // Truncated stream: runs out of input before the last row.
  decoder.begin(data.data(), data.size() / 2, entry.crc32);
  EXPECT_FALSE(decodeLevels(decoder, levels));

  // Flipped bits in the middle: either the inflate fails or the CRC does.
  for (size_t pos : {size_t{40}, data.size() / 3, data.size() / 2, data.size() - 20}) {
    std::vector<uint8_t> bad = data;
    bad[pos] ^= 0x5A;
    decoder.begin(bad.data(), bad.size(), entry.crc32);
    EXPECT_FALSE(decodeLevels(decoder, levels)) << "flip at " << pos;
  }

  // Intact data with the wrong expected CRC.
  decoder.begin(data.data(), data.size(), entry.crc32 ^ 1);
  EXPECT_FALSE(decodeLevels(decoder, levels));

  // Out-of-range card indexes.
  EXPECT_FALSE(decoder.beginCard(-1));
  EXPECT_EQ(decoder.nextRow(), nullptr);
  EXPECT_FALSE(decoder.beginCard(QUOTE_CARD_COUNT));
}

TEST(ChytankaQuoteCards, DarkModeInvertsLevels) {
  // 0 black, 1 dark gray, 2 light gray, 3 white.
  static_assert(quoteCardLevel(0, true) == 3 && quoteCardLevel(3, true) == 0);
  static_assert(quoteCardLevel(1, true) == 2 && quoteCardLevel(2, true) == 1);
  for (uint8_t level = 0; level < 4; level++) {
    EXPECT_EQ(quoteCardLevel(level, false), level);
    EXPECT_EQ(quoteCardLevel(quoteCardLevel(level, true), true), level);
  }

  // A whole inverted card keeps its gray pixels gray and swaps the histogram.
  QuoteCardDecoder decoder;
  std::vector<uint8_t> levels;
  ASSERT_TRUE(decoder.beginCard(0));
  ASSERT_TRUE(decodeLevels(decoder, levels));
  int normal[4] = {}, inverted[4] = {};
  for (const uint8_t level : levels) {
    normal[level]++;
    inverted[quoteCardLevel(level, true)]++;
  }
  for (int level = 0; level < 4; level++) EXPECT_EQ(inverted[level], normal[3 - level]);
}

// Mock framebuffer for one render pass over the card area: 1 = bit set
// (white), 0 = cleared (black), as GfxRenderer::drawPixel(state) leaves it.
struct MockPlane {
  std::vector<uint8_t> bits;
  long drawCalls = 0;
  void clear(const uint8_t byte) {
    bits.assign(static_cast<size_t>(QUOTE_CARD_WIDTH) * QUOTE_CARD_HEIGHT, byte ? 1 : 0);
  }
  void drawPixel(const int x, const int y, const bool black) {
    bits[static_cast<size_t>(y) * QUOTE_CARD_WIDTH + x] = black ? 0 : 1;
    drawCalls++;
  }
};

enum class Pass { BW, LSB, MSB };

void drawPixelForPass(MockPlane& plane, const Pass pass, const bool absolute, const int x, const int y,
                      const uint8_t level) {
  if (pass == Pass::BW) {
    plane.drawPixel(x, y, level < 3);
    return;
  }
  const GrayPlanePixel pixel = grayPlanePixel(level, pass == Pass::MSB, absolute);
  if (pixel.write) plane.drawPixel(x, y, pixel.black);
}

// The previous full draw: every pixel visited; B/W and absolute planes write
// every pixel, so they start from the opposite clear value to prove it.
void naivePass(const std::vector<uint8_t>& levels, MockPlane& plane, const Pass pass, const bool absolute,
               const bool inverted) {
  const uint8_t clear = quoteCardClearByte(pass != Pass::BW, absolute, inverted);
  const bool fullWrite = pass == Pass::BW || absolute;
  plane.clear(fullWrite ? static_cast<uint8_t>(~clear) : clear);
  for (int y = 0; y < QUOTE_CARD_HEIGHT; y++) {
    for (int x = 0; x < QUOTE_CARD_WIDTH; x++) {
      drawPixelForPass(plane, pass, absolute, x, y, quoteCardLevel(levels[y * QUOTE_CARD_WIDTH + x], inverted));
    }
  }
}

// Mirrors drawCardPass(): clear to quoteCardClearByte(), skip background rows
// and pixels.
bool skippingPass(QuoteCardDecoder& decoder, const int card, MockPlane& plane, const Pass pass, const bool absolute,
                  const bool inverted) {
  plane.clear(quoteCardClearByte(pass != Pass::BW, absolute, inverted));
  if (!decoder.beginCard(card)) return false;
  for (int y = 0; y < QUOTE_CARD_HEIGHT; y++) {
    const uint8_t* row = decoder.nextRow();
    if (!row) return false;
    if (quoteCardRowIsBackground(row)) continue;
    forEachQuoteCardForegroundPixel(row, 0, QUOTE_CARD_WIDTH, inverted, [&](const int x, const uint8_t level) {
      drawPixelForPass(plane, pass, absolute, x, y, level);
    });
  }
  return decoder.finish();
}

TEST(ChytankaQuoteCards, SkippingBackgroundMatchesFullDraw) {
  QuoteCardDecoder decoder;
  std::vector<uint8_t> levels;
  MockPlane naive, skipping;
  long naiveCalls = 0, skippingCalls = 0;
  for (int card = 0; card < QUOTE_CARD_COUNT; card++) {
    ASSERT_TRUE(decoder.beginCard(card));
    ASSERT_TRUE(decodeLevels(decoder, levels));
    for (const bool inverted : {false, true}) {
      for (const bool absolute : {false, true}) {
        for (const Pass pass : {Pass::BW, Pass::LSB, Pass::MSB}) {
          if (pass == Pass::BW && absolute) continue;  // B/W does not depend on it
          naive.drawCalls = skipping.drawCalls = 0;
          naivePass(levels, naive, pass, absolute, inverted);
          ASSERT_TRUE(skippingPass(decoder, card, skipping, pass, absolute, inverted));
          ASSERT_EQ(naive.bits, skipping.bits) << "card " << card << " pass " << static_cast<int>(pass) << " absolute "
                                               << absolute << " inverted " << inverted;
          naiveCalls += naive.drawCalls;
          skippingCalls += skipping.drawCalls;
        }
      }
    }
  }
  EXPECT_LT(skippingCalls * 5, naiveCalls);
  printf("drawPixel calls, all cards/passes: full draw %ld, skipping %ld (%.1f%%)\n", naiveCalls, skippingCalls,
         100.0 * static_cast<double>(skippingCalls) / static_cast<double>(naiveCalls));
}

TEST(ChytankaQuotePicker, EachCycleShowsEveryCardOnce) {
  for (size_t i = 0; i < 4096; i++) sequence[i] = static_cast<uint32_t>(i * 2654435761u >> 7);
  sequencePos = 0;
  QuoteCardHistory history;
  int previous = -1;
  for (int cycle = 0; cycle < 20; cycle++) {
    std::vector<int> seen(QUOTE_CARD_COUNT, 0);
    for (int i = 0; i < QUOTE_CARD_COUNT; i++) {
      const int card = pickQuoteCard(history, QUOTE_CARD_COUNT, scriptedRandom);
      ASSERT_GE(card, 0);
      ASSERT_LT(card, QUOTE_CARD_COUNT);
      ASSERT_NE(card, previous) << "back-to-back repeat in cycle " << cycle;
      previous = card;
      seen[card]++;
    }
    // Every window of QUOTE_CARD_COUNT picks is one full bag: each card once.
    for (const int n : seen) EXPECT_EQ(n, 1);
  }
}

TEST(ChytankaQuotePicker, NoRepeatWithinFiftyPicksEvenAtCycleBoundary) {
  QuoteCardHistory history;
  std::vector<int> picks;
  for (int i = 0; i < 500; i++) picks.push_back(pickQuoteCard(history, QUOTE_CARD_COUNT, lastRandom));
  for (size_t i = 1; i < picks.size(); i++) EXPECT_NE(picks[i], picks[i - 1]) << i;
}

TEST(ChytankaQuotePicker, InconsistentHistoryRestartsTheBag) {
  QuoteCardHistory history;
  history.count = 12;  // built for another card set
  history.shown = ~uint64_t{0};
  history.last = 200;
  EXPECT_EQ(pickQuoteCard(history, QUOTE_CARD_COUNT, firstRandom), 0);
  EXPECT_EQ(history.count, QUOTE_CARD_COUNT);
  EXPECT_EQ(history.shown, uint64_t{1});
  EXPECT_EQ(history.last, 0);

  history.shown |= uint64_t{1} << 63;                                   // stray bit beyond the card count
  EXPECT_EQ(pickQuoteCard(history, QUOTE_CARD_COUNT, firstRandom), 0);  // full reset, including last
  EXPECT_EQ(history.shown, uint64_t{1});
}

TEST(ChytankaQuotePicker, EdgeCounts) {
  QuoteCardHistory history;
  for (int i = 0; i < 3; i++) EXPECT_EQ(pickQuoteCard(history, 1, lastRandom), 0);
  history = {};
  for (int i = 0; i < 128; i++) {
    const int card = pickQuoteCard(history, 64, outOfRangeRandom);
    EXPECT_GE(card, 0);
    EXPECT_LT(card, 64);
  }
  EXPECT_EQ(pickQuoteCard(history, 0, firstRandom), -1);
  EXPECT_EQ(pickQuoteCard(history, 65, firstRandom), -1);
  EXPECT_EQ(pickQuoteCard(history, 5, nullptr), -1);
}
