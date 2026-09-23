// Sharp B/W text threshold: text that no grayscale pass refines drops
// light-gray glyph coverage (Chytanka). Built twice: with CHYTANKA the reader
// flag applies, without it stock rendering must be unchanged.
#include <GfxRenderer.h>
#include <gtest/gtest.h>

#include <set>
#include <utility>

namespace {
// One glyph 'A': a single row of four pixels, 2-bit raw values 3,2,1,0
// (black, dark gray, light gray, white) at x offsets 0..3.
struct FourLevelFont {
  EpdUnicodeInterval interval{'A', 'A', 0};
  EpdGlyph glyph{};
  uint8_t pixels[1] = {0xE4};
  EpdFontData data{};
  EpdFont font{&data};
  FourLevelFont() {
    glyph.width = 4;
    glyph.height = 1;
    glyph.advanceX = 5 * 16;
    glyph.left = 0;
    glyph.top = 1;
    glyph.dataOffset = 0;
    glyph.dataLength = 1;
    data.bitmap = pixels;
    data.glyph = &glyph;
    data.intervals = &interval;
    data.intervalCount = 1;
    data.advanceY = 8;
    data.ascender = 6;
    data.descender = -2;
    data.is2Bit = true;
  }
};

std::set<std::pair<int, int>> inkedPixels(GfxRenderer& renderer) {
  std::set<std::pair<int, int>> inked;
  for (int y = 0; y < 80; ++y)
    for (int x = 0; x < 80; ++x)
      if (renderer.isPixelBlack(x, y)) inked.insert({x, y});
  return inked;
}

std::set<int> inkedColumns(const std::set<std::pair<int, int>>& inked) {
  std::set<int> xs;
  for (const auto& p : inked) xs.insert(p.first);
  return xs;
}
}  // namespace

TEST(SharpBwText, ThresholdMapping) {
  // Default B/W base: every non-white pixel is ink.
  EXPECT_TRUE(GfxRenderer::isBwGlyphInk(0, false));
  EXPECT_TRUE(GfxRenderer::isBwGlyphInk(1, false));
  EXPECT_TRUE(GfxRenderer::isBwGlyphInk(2, false));
  EXPECT_FALSE(GfxRenderer::isBwGlyphInk(3, false));
  // Sharp B/W-only: black and dark gray are ink, light gray and white are not.
  EXPECT_TRUE(GfxRenderer::isBwGlyphInk(0, true));
  EXPECT_TRUE(GfxRenderer::isBwGlyphInk(1, true));
  EXPECT_FALSE(GfxRenderer::isBwGlyphInk(2, true));
  EXPECT_FALSE(GfxRenderer::isBwGlyphInk(3, true));
}

TEST(SharpBwText, ReaderUsesSharpThresholdOnlyWithAntiAliasingOff) {
  // Anti-aliasing off: every page is B/W only, so text gets the sharp threshold.
  EXPECT_TRUE(GfxRenderer::sharpBwTextForReader(false));
  // Anti-aliasing on: the full black base stays, including the pages that get
  // no grayscale pass (white text on a dark background, queued intermediate
  // pages), so they keep the same stroke weight as anti-aliased pages.
  EXPECT_FALSE(GfxRenderer::sharpBwTextForReader(true));
}

TEST(SharpBwText, DarkBackgroundWithAntiAliasingKeepsFullBase) {
  // White-on-black text (dark reader background) with anti-aliasing on gets
  // no grayscale pass but must still draw every non-white glyph pixel.
  FourLevelFont fixture;
  HalDisplay display;
  GfxRenderer renderer(display);
  renderer.begin();
  renderer.insertFont(1, EpdFontFamily(&fixture.font));
  renderer.setRenderMode(GfxRenderer::BW);

  const auto drawWhiteOnBlack = [&](const bool sharp) {
    renderer.clearScreen(0x00);
    GfxRenderer::SharpBwTextScope scope(renderer, sharp);
    renderer.drawText(1, 20, 20, "A", /*black=*/false);
    std::set<std::pair<int, int>> lit;
    for (int y = 0; y < 80; ++y)
      for (int x = 0; x < 80; ++x)
        if (!renderer.isPixelBlack(x, y)) lit.insert({x, y});
    return lit;
  };

  const auto fullBase = drawWhiteOnBlack(false);
  const auto withAntiAliasing = drawWhiteOnBlack(GfxRenderer::sharpBwTextForReader(true));
  EXPECT_EQ(withAntiAliasing, fullBase);
  EXPECT_EQ(inkedColumns(fullBase).size(), 3u);  // black, dark and light gray pixels
}

TEST(SharpBwText, ScopeRestoresPreviousState) {
  HalDisplay display;
  GfxRenderer renderer(display);
  {
    GfxRenderer::SharpBwTextScope outer(renderer, true);
    {
      GfxRenderer::SharpBwTextScope inner(renderer, false);
      EXPECT_FALSE(renderer.sharpBwTextEnabled());
    }
#ifdef CHYTANKA
    EXPECT_TRUE(renderer.sharpBwTextEnabled());
#else
    EXPECT_FALSE(renderer.sharpBwTextEnabled());
#endif
  }
  EXPECT_FALSE(renderer.sharpBwTextEnabled());
}

TEST(SharpBwText, RasterizedGlyphUsesThresholdOnlyInBwMode) {
  FourLevelFont fixture;
  HalDisplay display;
  GfxRenderer renderer(display);
  renderer.begin();
  renderer.insertFont(1, EpdFontFamily(&fixture.font));

  for (const auto style : {EpdFontFamily::REGULAR, EpdFontFamily::SMALL_CAPS}) {
    SCOPED_TRACE(testing::Message() << "style=" << style);
    const auto draw = [&](const GfxRenderer::RenderMode mode, const bool sharp) {
      renderer.setRenderMode(mode);
      renderer.clearScreen(mode == GfxRenderer::BW ? 0xFF : 0x00);
      GfxRenderer::SharpBwTextScope scope(renderer, sharp);
      renderer.drawText(1, 20, 20, "A", true, style);
      std::vector<uint8_t> frame(display.bw);
      renderer.setRenderMode(GfxRenderer::BW);
      return frame;
    };

    draw(GfxRenderer::BW, false);
    const auto base = inkedPixels(renderer);
    draw(GfxRenderer::BW, true);
    const auto sharp = inkedPixels(renderer);
    ASSERT_FALSE(base.empty());

    if (style == EpdFontFamily::REGULAR) {
      const auto baseXs = inkedColumns(base);
      ASSERT_EQ(baseXs.size(), 3u);  // black, dark gray, light gray
#ifdef CHYTANKA
      const int x0 = *baseXs.begin();
      EXPECT_EQ(inkedColumns(sharp), (std::set<int>{x0, x0 + 1}));  // light gray dropped
#else
      EXPECT_EQ(sharp, base);  // stock builds ignore the flag
#endif
    } else {
#ifdef CHYTANKA
      EXPECT_LT(sharp.size(), base.size());
      for (const auto& p : sharp) EXPECT_TRUE(base.count(p));
#else
      EXPECT_EQ(sharp, base);
#endif
    }

    // Grayscale planes never depend on the flag.
    for (const auto mode : {GfxRenderer::GRAYSCALE_LSB, GfxRenderer::GRAYSCALE_MSB}) {
      EXPECT_EQ(draw(mode, false), draw(mode, true)) << "mode=" << mode;
    }
  }
  renderer.removeFont(1);
}
