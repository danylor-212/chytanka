#include <gtest/gtest.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include "ChytankaQuoteDecoder.h"

using chytanka::pickQuoteCard;
using chytanka::QUOTE_CARD_COUNT;
using chytanka::QUOTE_CARD_DATA;
using chytanka::QUOTE_CARD_HEIGHT;
using chytanka::QUOTE_CARD_ROW_BYTES;
using chytanka::QUOTE_CARD_WIDTH;
using chytanka::QUOTE_CARDS;
using chytanka::QuoteCardDecoder;
using chytanka::QuoteCardHistory;

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

TEST(ChytankaQuoteCards, CardsUseGrayLevelsAndWhiteMargins) {
  QuoteCardDecoder decoder;
  std::vector<uint8_t> levels;
  ASSERT_TRUE(decoder.beginCard(0));
  ASSERT_TRUE(decodeLevels(decoder, levels));
  int histogram[4] = {};
  for (const uint8_t level : levels) histogram[level]++;
  EXPECT_GT(histogram[0], 0);                       // black text
  EXPECT_GT(histogram[1] + histogram[2], 0);        // anti-aliasing / ornament grays
  EXPECT_GT(histogram[3], QUOTE_CARD_WIDTH * 400);  // mostly white card
  // The X3 (528x792) clips 4 rows top and bottom: they must be blank.
  for (int y : {0, 1, 2, 3, QUOTE_CARD_HEIGHT - 4, QUOTE_CARD_HEIGHT - 1}) {
    for (int x = 0; x < QUOTE_CARD_WIDTH; x++) ASSERT_EQ(levels[y * QUOTE_CARD_WIDTH + x], 3) << x << "," << y;
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

// Writes each decoded card as a PGM for visual checks when requested.
TEST(ChytankaQuoteCards, DumpPgmWhenRequested) {
  const char* dir = std::getenv("CHYTANKA_QUOTE_DUMP_DIR");
  if (!dir || !*dir) GTEST_SKIP() << "CHYTANKA_QUOTE_DUMP_DIR not set";
  QuoteCardDecoder decoder;
  std::vector<uint8_t> levels;
  for (int i = 0; i < QUOTE_CARD_COUNT; i++) {
    ASSERT_TRUE(decoder.beginCard(i));
    ASSERT_TRUE(decodeLevels(decoder, levels));
    char path[512];
    snprintf(path, sizeof(path), "%s/card%02d_q%03u.pgm", dir, i, QUOTE_CARDS[i].quoteId);
    std::ofstream out(path, std::ios::binary);
    out << "P5\n" << QUOTE_CARD_WIDTH << " " << QUOTE_CARD_HEIGHT << "\n255\n";
    for (const uint8_t level : levels) out.put(static_cast<char>(level * 85));
    ASSERT_TRUE(out.good()) << path;
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
