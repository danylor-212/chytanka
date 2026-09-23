#ifdef CHYTANKA

#include "ChytankaBrand.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include "fontIds.h"
#include "images/ChytankaLogo120.h"

#if defined(CHYTANKA) && !defined(CROSSINK_OTA_RELEASE_URL)
#error "Chytanka builds must override CROSSINK_OTA_RELEASE_URL"
#endif

namespace chytanka {

namespace {

static_assert(sizeof(ChytankaLogo120) == 120 * 120 / 8, "ChytankaLogo120 must be 120x120 1-bit");

// The brand name is the same in every UI language. The credit line is kept
// out of the shared translation files so CrossInk rebases stay conflict-free;
// any UI language other than Ukrainian falls back to the English credit.
constexpr const char* BRAND_NAME = "Читанка";
constexpr const char* BASED_ON_UK = "на основі CrossInk";
constexpr const char* BASED_ON_EN = "based on CrossInk";

}  // namespace

void drawBrandBlock(const GfxRenderer& renderer, const int pageWidth, const int pageHeight, const char* status) {
  const char* basedOn = I18N.getLanguage() == Language::UK ? BASED_ON_UK : BASED_ON_EN;

  renderer.drawImage(ChytankaLogo120, (pageWidth - 120) / 2, (pageHeight - 120) / 2, 120, 120);
  renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 + 70, BRAND_NAME, true, EpdFontFamily::BOLD);
  renderer.drawCenteredText(SMALL_FONT_ID, pageHeight / 2 + 95, basedOn);
  renderer.drawCenteredText(SMALL_FONT_ID, pageHeight / 2 + 120, status);
}

}  // namespace chytanka

#endif  // CHYTANKA
