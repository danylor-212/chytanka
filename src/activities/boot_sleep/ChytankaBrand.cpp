#ifdef CHYTANKA

#include "ChytankaBrand.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include "fontIds.h"
#include "images/ChytankaLogo240.h"

#if defined(CHYTANKA) && !defined(CROSSINK_OTA_RELEASE_URL)
#error "Chytanka builds must override CROSSINK_OTA_RELEASE_URL"
#endif

namespace chytanka {

namespace {

constexpr int LOGO_SIZE = 240;
static_assert(sizeof(ChytankaLogo240) == LOGO_SIZE * LOGO_SIZE / 8, "ChytankaLogo240 must be 240x240 1-bit");

// Layout (portrait only; the boot and sleep screens never run in landscape).
// The logo sits LOGO_LIFT px above the vertical centre so the text block below
// it stays visually balanced. On X4 (480x800) the logo box is y=240..480 and
// the status line ends near y=588; on X3 (528x792) 236..476 and ~584. Both
// stay well clear of the boot version line at H-30.
constexpr int LOGO_LIFT = 40;
constexpr int NAME_GAP = 24;    // logo bottom -> brand name
constexpr int CREDIT_GAP = 10;  // brand name line -> credit
constexpr int STATUS_GAP = 6;   // credit line -> status

// The brand name is the same in every UI language. The credit line is kept
// out of the shared translation files so CrossInk rebases stay conflict-free;
// any UI language other than Ukrainian falls back to the English credit.
constexpr const char* BRAND_NAME = "Читанка";
constexpr const char* BASED_ON_UK = "на основі CrossInk";
constexpr const char* BASED_ON_EN = "based on CrossInk";

}  // namespace

void drawBrandBlock(const GfxRenderer& renderer, const int pageWidth, const int pageHeight, const char* status) {
  const char* basedOn = I18N.getLanguage() == Language::UK ? BASED_ON_UK : BASED_ON_EN;

  const int logoY = (pageHeight - LOGO_SIZE) / 2 - LOGO_LIFT;
  const int nameY = logoY + LOGO_SIZE + NAME_GAP;
  const int creditY = nameY + renderer.getLineHeight(UI_12_FONT_ID) + CREDIT_GAP;
  const int statusY = creditY + renderer.getLineHeight(SMALL_FONT_ID) + STATUS_GAP;

  renderer.drawImage(ChytankaLogo240, (pageWidth - LOGO_SIZE) / 2, logoY, LOGO_SIZE, LOGO_SIZE);
  renderer.drawCenteredText(UI_12_FONT_ID, nameY, BRAND_NAME, true, EpdFontFamily::BOLD);
  renderer.drawCenteredText(SMALL_FONT_ID, creditY, basedOn);
  renderer.drawCenteredText(SMALL_FONT_ID, statusY, status);
}

}  // namespace chytanka

#endif  // CHYTANKA
