#ifdef CHYTANKA

#include "ChytankaQuoteSleep.h"

#include <Arduino.h>
#include <BitmapHelpers.h>
#include <GfxRenderer.h>
#include <HalDisplay.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

#include <algorithm>
#include <cstring>

#include "ChytankaQuoteDecoder.h"
#include "ChytankaReadingLine.h"
#include "CrossPointSettings.h"
#include "fontIds.h"

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
// The caller must clear the pass to quoteCardClearByte(): background pixels
// are skipped, not drawn.
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
    // Background pixels already hold their final value from the pass's
    // clearScreen(quoteCardClearByte(...)), so only foreground is drawn.
    if (y < 0 || y >= screenHeight || quoteCardRowIsBackground(row)) continue;

    forEachQuoteCardForegroundPixel(row, firstX, endX, inverted, [&](const int cx, const uint8_t level) {
      if (mode == GfxRenderer::BW) {
        renderer.drawPixel(x0 + cx, y, level < 3);
      } else {
        const GrayPlanePixel pixel = grayPlanePixel(level, msb, absolute);
        if (pixel.write) renderer.drawPixel(x0 + cx, y, pixel.black);
      }
    });
  }
  return decoder.finish();
}

// "Currently reading" line, in card coordinates. The cards
// (brand/quotes/make_cards.py) put their text at x = 44 and the brand footer
// (logo + wordmark) from y = H - 62 = 738 down to 770; the quote and
// attribution end at y = 683 at the latest (all 50 cards). The line sits in
// between, left-aligned with the text, in the card's secondary style: Bitter
// italic in the level the attribution uses (1 = dark gray, light on a dark
// card). Rows 796-799 stay background (cropped on the X3).
constexpr int READING_LINE_FONT_ID = BITTER_10_FONT_ID;
constexpr EpdFontFamily::Style READING_LINE_STYLE = EpdFontFamily::ITALIC;
constexpr int READING_LINE_X = 44;
constexpr int READING_LINE_RIGHT = QUOTE_CARD_WIDTH - 44;
constexpr int READING_LINE_FOOTER_TOP = QUOTE_CARD_HEIGHT - 62;
constexpr int READING_LINE_FOOTER_GAP = 14;
constexpr uint8_t READING_LINE_LEVEL = 1;

// Draws the line for the renderer's current pass so that it ends up at
// `level` like a card pixel of that level would: the B/W pass inks every
// non-white level; gray passes write the plane bits grayPlanePixel() gives.
// Glyphs are drawn solid (no anti-aliasing): the renderer draws text in the
// B/W style whenever the gray planes are absolute, and a relative plane is
// written with the render mode briefly set to B/W (setRenderMode() only does
// extra work when leaving absolute planes, which this path never does).
void drawReadingLinePass(GfxRenderer& renderer, const std::string& line, const int x, const int y,
                         const uint8_t level) {
  if (line.empty()) return;
  const GfxRenderer::RenderMode mode = renderer.getRenderMode();
  if (mode == GfxRenderer::BW) {
    renderer.drawText(READING_LINE_FONT_ID, x, y, line.c_str(), level < 3, READING_LINE_STYLE);
    return;
  }
  const bool absolute = renderer.grayPlanesAreAbsolute();
  const GrayPlanePixel pixel = grayPlanePixel(level, mode == GfxRenderer::GRAYSCALE_MSB, absolute);
  if (!pixel.write) return;
  if (absolute) {
    renderer.drawText(READING_LINE_FONT_ID, x, y, line.c_str(), pixel.black, READING_LINE_STYLE);
    return;
  }
  renderer.setRenderMode(GfxRenderer::BW);
  renderer.drawText(READING_LINE_FONT_ID, x, y, line.c_str(), pixel.black, READING_LINE_STYLE);
  renderer.setRenderMode(mode);
}

}  // namespace

bool renderQuoteCardSleepScreen(GfxRenderer& renderer, const bool turnOffScreen, const std::string& readingTitle,
                                const float progressPercent) {
  // Cards are portrait-only. SleepActivity sets Portrait before drawing; if a
  // future path does not, fall back to the brand block (which lays out in any
  // orientation) rather than changing the renderer's orientation behind the
  // caller's back.
  if (renderer.getScreenWidth() > renderer.getScreenHeight()) {
    LOG_INF("SLP", "Quote card skipped: renderer is not in portrait");
    return false;
  }

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
  const auto filter = SETTINGS.sleepScreenCoverFilter;

  // Built once, drawn in every pass. With a black & white cover filter there
  // are no gray passes, so the line is drawn in full ink instead of gray (a
  // gray line on a dark card would otherwise vanish in the B/W pass).
  const int readingMaxWidth = std::min(READING_LINE_RIGHT, renderer.getScreenWidth() - x0) - READING_LINE_X;
  const std::string readingLine = buildReadingLine(
      readingTitle, readingLinePercent(progressPercent), I18N.getLanguage() == Language::UK,
      [&](const std::string& candidate) {
        return renderer.getTextWidth(READING_LINE_FONT_ID, candidate.c_str(), READING_LINE_STYLE) <= readingMaxWidth;
      });
  const uint8_t readingLevel = quoteCardLevel(
      filter == CrossPointSettings::SLEEP_SCREEN_COVER_FILTER::NO_FILTER ? READING_LINE_LEVEL : 0, inverted);
  const int readingX = x0 + READING_LINE_X;
  const int readingY =
      y0 + READING_LINE_FOOTER_TOP - READING_LINE_FOOTER_GAP - renderer.getLineHeight(READING_LINE_FONT_ID);
  if (!readingLine.empty()) LOG_INF("SLP", "Quote card reading line: %s", readingLine.c_str());

  // Background (and margins) = clear value, see quoteCardClearByte().
  renderer.clearScreen(quoteCardClearByte(false, false, inverted));
  if (!drawCardPass(renderer, *decoder, card, x0, y0, inverted)) {
    LOG_ERR("SLP", "Quote card %d (id %u) failed to decode", card, QUOTE_CARDS[card].quoteId);
    return false;
  }
  drawReadingLinePass(renderer, readingLine, readingX, readingY, readingLevel);
  LOG_INF("SLP", "Quote card %d (id %u), B/W pass %lu ms, free heap %u", card, QUOTE_CARDS[card].quoteId,
          millis() - startMs, ESP.getFreeHeap());

  // From here on: the display sequence of SleepActivity::renderBitmapSleepScreen().
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
    // Absolute planes are complete, so the background and margins must be
    // present in both: 0xFF white / 0x00 black. Relative planes only mark gray
    // pixels and start empty. Either way the background is never drawn.
    renderer.clearScreen(quoteCardClearByte(true, absolute, inverted));
    renderer.setRenderMode(mode);
    if (!drawCardPass(renderer, *decoder, card, x0, y0, inverted)) {
      LOG_ERR("SLP", "Quote card %d failed to decode in a grayscale pass", card);
      renderer.setRenderMode(GfxRenderer::BW);
      return false;
    }
    drawReadingLinePass(renderer, readingLine, readingX, readingY, readingLevel);
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
