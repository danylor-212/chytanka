#ifdef CHYTANKA

#include "ChytankaQuoteSleep.h"

#include <Arduino.h>
#include <BitmapHelpers.h>
#include <GfxRenderer.h>
#include <HalDisplay.h>
#include <HalStorage.h>
#include <Logging.h>
#include <Memory.h>

#include <algorithm>
#include <cstring>

#include "ChytankaQuoteDecoder.h"
#include "CrossPointSettings.h"

namespace chytanka {

namespace {

// Kept apart from APP_STATE's recentSleepImages, which index the SD sleep
// folder: a separate file needs no hooks in CrossPointState.
constexpr char HISTORY_FILE[] = "/.crosspoint/chytanka_quotes.bin";
constexpr uint8_t HISTORY_MAGIC0 = 'C';
constexpr uint8_t HISTORY_MAGIC1 = 'Q';
constexpr uint8_t HISTORY_VERSION = 1;
// magic[2], version, count, last, reserved[3], shown (uint64 little-endian)
constexpr size_t HISTORY_SIZE = 16;

uint32_t randomBelow(const uint32_t n) { return static_cast<uint32_t>(random(static_cast<long>(n))); }

QuoteCardHistory loadHistory() {
  QuoteCardHistory history;
  if (!Storage.exists(HISTORY_FILE)) return history;

  FsFile file;
  if (!Storage.openFileForRead("SLP", HISTORY_FILE, file)) return history;
  uint8_t buf[HISTORY_SIZE];
  const int read = file.read(buf, sizeof(buf));
  file.close();
  if (read != static_cast<int>(sizeof(buf)) || buf[0] != HISTORY_MAGIC0 || buf[1] != HISTORY_MAGIC1 ||
      buf[2] != HISTORY_VERSION) {
    LOG_ERR("SLP", "Ignoring invalid quote card history");
    return history;
  }
  history.count = buf[3];
  history.last = buf[4];
  for (int i = 7; i >= 0; i--) history.shown = (history.shown << 8) | buf[8 + i];
  return history;  // pickQuoteCard() resets anything inconsistent
}

void saveHistory(const QuoteCardHistory& history) {
  uint8_t buf[HISTORY_SIZE] = {HISTORY_MAGIC0, HISTORY_MAGIC1, HISTORY_VERSION, history.count, history.last};
  for (int i = 0; i < 8; i++) buf[8 + i] = static_cast<uint8_t>(history.shown >> (8 * i));

  FsFile file;
  if (!Storage.openFileForWrite("SLP", HISTORY_FILE, file)) {
    LOG_ERR("SLP", "Failed to open %s for writing", HISTORY_FILE);
    return;
  }
  if (file.write(buf, sizeof(buf)) != sizeof(buf)) LOG_ERR("SLP", "Short write to %s", HISTORY_FILE);
  file.close();
}

// Draws the whole card for the renderer's current mode with the same per-pixel
// rule as GfxRenderer::drawBitmap() for a 2bpp row, so the result matches the
// card as an SD sleep BMP. The card is centred without scaling: on the X3
// (528x792) that leaves 24 px white side margins and clips 4 blank rows at
// the top and bottom of the card. Margins get the card's background level.
bool drawCardPass(GfxRenderer& renderer, QuoteCardDecoder& decoder, const int card, const int x0, const int y0,
                  const bool inverted) {
  if (!decoder.beginCard(card)) return false;

  const GfxRenderer::RenderMode mode = renderer.getRenderMode();
  const bool msb = mode == GfxRenderer::GRAYSCALE_MSB;
  const bool absolute = renderer.grayPlanesAreAbsolute();
  const int screenWidth = renderer.getScreenWidth();
  const int screenHeight = renderer.getScreenHeight();
  const int firstX = x0 < 0 ? -x0 : 0;
  const int endX = std::min(QUOTE_CARD_WIDTH, screenWidth - x0);

  for (int cy = 0; cy < QUOTE_CARD_HEIGHT; cy++) {
    const uint8_t* row = decoder.nextRow();
    if (!row) return false;
    const int y = y0 + cy;
    if (y < 0 || y >= screenHeight) continue;

    for (int cx = firstX; cx < endX; cx++) {
      const uint8_t level = quoteCardLevel((row[cx >> 2] >> (6 - ((cx & 3) << 1))) & 0x3, inverted);
      if (mode == GfxRenderer::BW) {
        // Write white too: the inverted card's background is black.
        renderer.drawPixel(x0 + cx, y, level < 3);
      } else {
        const GrayPlanePixel pixel = grayPlanePixel(level, msb, absolute);
        if (pixel.write) renderer.drawPixel(x0 + cx, y, pixel.black);
      }
    }
  }
  return decoder.finish();
}

}  // namespace

bool renderQuoteCardSleepScreen(GfxRenderer& renderer, const bool turnOffScreen) {
  // One ~1.9 KB block (inflate state + 512 B ring + one row) for the whole
  // render; freed on return. Everything else is flash or the framebuffer.
  auto decoder = makeUniqueNoThrow<QuoteCardDecoder>();
  if (!decoder) {
    LOG_ERR("SLP", "OOM: quote card decoder (%u B; free %u, max alloc %u)",
            static_cast<unsigned>(sizeof(QuoteCardDecoder)), ESP.getFreeHeap(), ESP.getMaxAllocHeap());
    return false;
  }

  // Record the pick before drawing: a card that fails to decode is then
  // skipped for the rest of the cycle instead of being retried every sleep.
  QuoteCardHistory history = loadHistory();
  const int card = pickQuoteCard(history, QUOTE_CARD_COUNT, randomBelow);
  saveHistory(history);

  const int x0 = (renderer.getScreenWidth() - QUOTE_CARD_WIDTH) / 2;
  const int y0 = (renderer.getScreenHeight() - QUOTE_CARD_HEIGHT) / 2;
  const unsigned long startMs = millis();
  // Dark (CrossInk's default) shows the card inverted; Light as designed. The
  // levels are swapped in every pass, so the gray planes stay correct.
  const bool inverted = SETTINGS.sleepScreen != CrossPointSettings::SLEEP_SCREEN_MODE::LIGHT;

  renderer.clearScreen(inverted ? 0x00 : 0xFF);
  if (!drawCardPass(renderer, *decoder, card, x0, y0, inverted)) {
    LOG_ERR("SLP", "Quote card %d (id %u) failed to decode", card, QUOTE_CARDS[card].quoteId);
    return false;
  }
  LOG_INF("SLP", "Quote card %d (id %u), B/W pass %lu ms, free heap %u", card, QUOTE_CARDS[card].quoteId,
          millis() - startMs, ESP.getFreeHeap());

  // From here on: the display sequence of SleepActivity::renderBitmapSleepScreen().
  const auto filter = SETTINGS.sleepScreenCoverFilter;
  if (filter == CrossPointSettings::SLEEP_SCREEN_COVER_FILTER::INVERTED_BLACK_AND_WHITE) {
    renderer.invertScreen();
  }
  if (filter != CrossPointSettings::SLEEP_SCREEN_COVER_FILTER::NO_FILTER) {
    renderer.displayBuffer(HalDisplay::HALF_REFRESH, turnOffScreen);
    return true;
  }

  const bool absolute = renderer.supportsAbsoluteGrayscale();
  const bool direct = absolute && renderer.supportsDirectGrayscale();
  if (absolute) {
    if (!(direct ? renderer.displayDirectGrayscaleBase() : renderer.displayAbsoluteGrayscaleBase())) return false;
  } else {
    renderer.displayGrayscaleBase(HalDisplay::HALF_REFRESH);
  }

  for (const auto mode : {GfxRenderer::GRAYSCALE_LSB, GfxRenderer::GRAYSCALE_MSB}) {
    // Absolute margins must be present in both complete planes: white is
    // 0xFF there, black (the inverted card's background) 0x00. Relative
    // planes only mark gray pixels, so they start empty.
    renderer.clearScreen(absolute && !inverted ? 0xFF : 0x00);
    renderer.setRenderMode(mode);
    if (!drawCardPass(renderer, *decoder, card, x0, y0, inverted)) {
      LOG_ERR("SLP", "Quote card %d failed to decode in a grayscale pass", card);
      renderer.setRenderMode(GfxRenderer::BW);
      return false;
    }
    if (mode == GfxRenderer::GRAYSCALE_LSB) {
      renderer.copyGrayscaleLsbBuffers();
    } else {
      renderer.copyGrayscaleMsbBuffers();
    }
  }
  renderer.displayGrayBuffer(turnOffScreen);
  renderer.setRenderMode(GfxRenderer::BW);
  LOG_INF("SLP", "Quote card rendered in %lu ms", millis() - startMs);
  return true;
}

}  // namespace chytanka

#endif  // CHYTANKA
