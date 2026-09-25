#ifdef CHYTANKA

#include "ChytankaWelcomeActivity.h"

#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>

#include <cstring>
#include <string>

#include "ChytankaWelcome.h"
#include "CrossPointSettings.h"
#include "CrossPointState.h"
#include "MappedInputManager.h"
#include "OpdsServerStore.h"
#include "activities/ActivityManager.h"
#include "activities/RenderLock.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace chytanka {

namespace {

// Fork-owned marker: present once the welcome was answered (or found
// unnecessary). A separate file needs no hooks in CrossPointSettings, and
// stock CrossInk leaves it alone, so flashing stock and back does not re-ask.
constexpr char MARKER_FILE[] = "/.crosspoint/chytanka_welcome.txt";
// Written only by earlier Chytanka releases (ChytankaQuoteSleep.cpp's quote
// shuffle bag, OpdsServerStore's seed flag), never by CrossInk.
constexpr char QUOTE_HISTORY_FILE[] = "/.crosspoint/chytanka_quotes.bin";
constexpr char OPDS_SEEDED_KEY[] = "\"chytankaCatalogueSeeded\"";
// Written on every boot of 1.6.0.1 and later (BookCacheUtils.cpp's one-time
// cover-thumbnail reset marker), before Home; not by stock CrossInk. Catches a
// 1.6.0.1 device that never slept on a quote card and whose OPDS list was
// full, so neither trace above exists.
constexpr char THUMB_FORMAT_MARKER_FILE[] = "/.crosspoint/thumb_format.bin";
// CrossPointSettings.cpp: CrossInk's JSON, the CrossPoint JSON it migrates
// from, and the older binary format (renamed to .bak after migration).
constexpr const char* LEGACY_SETTINGS_FILES[] = {"/.crosspoint/settings.json", "/.crosspoint/settings.bin",
                                                 "/.crosspoint/settings.bin.bak"};

// Screen text. Shown before any language switch, so every line is given in
// Ukrainian and English; kept out of the shared translation files so CrossInk
// rebases stay conflict-free.
constexpr const char* TITLE_UK = "Ласкаво просимо до Читанки";
constexpr const char* TITLE_EN = "Welcome to Chytanka";
constexpr const char* INTRO_UK = "Читанку встановлено поверх ваших налаштувань CrossInk. Що змінити?";
constexpr const char* INTRO_EN = "Chytanka was installed over your CrossInk settings. What should change?";
constexpr const char* LANGUAGE_UK = "Мова інтерфейсу: українська";
constexpr const char* LANGUAGE_EN = "Interface language: Ukrainian";
constexpr const char* READING_UK = "Рекомендоване для читання";
constexpr const char* READING_EN = "Reading: Bitter, hyphenation, no text anti-aliasing";
constexpr const char* READING_DETAIL_UK = "Bitter, переноси, без згладжування тексту";
constexpr const char* READING_SD_UK = "Переноси, без згладжування тексту";
constexpr const char* READING_SD_EN = "Hyphenation, no text anti-aliasing";
constexpr const char* READING_SD_DETAIL_UK = "Ваш шрифт з картки залишиться";
constexpr const char* APPLY_UK = "Застосувати";
constexpr const char* APPLY_EN = "Apply";
constexpr const char* LATER_UK = "Пізніше";
constexpr const char* LATER_EN = "Later";
constexpr const char* FOOTNOTE_UK = "Книжки з власними налаштуваннями їх збережуть. Усе можна змінити в Налаштуваннях.";
constexpr const char* FOOTNOTE_EN =
    "Books with their own reader settings keep them. Change anything later in Settings.";

constexpr int OPTION_PADDING = 12;
constexpr int CHECKBOX_SIZE = 22;
constexpr int BUTTON_HEIGHT = 64;
constexpr int SECTION_GAP = 22;

bool languageIsUkrainian() { return SETTINGS.language == static_cast<uint8_t>(Language::UK); }

bool usesSdFont() { return SETTINGS.sdFontFamilyName[0] != '\0'; }

// An SD-card font is the reader's own choice, so it counts as recommended.
bool readingIsRecommended() {
  const bool fontOk =
      usesSdFont() || CrossPointSettings::availableBuiltinFont(SETTINGS.fontFamily) == CrossPointSettings::BITTER;
  return fontOk && SETTINGS.hyphenationEnabled != 0 && SETTINGS.textAntiAliasing == 0;
}

bool anySettingsFileExists() {
  if (Storage.exists(CrossPointSettings::getFilePath())) return true;
  for (const char* path : LEGACY_SETTINGS_FILES) {
    if (Storage.exists(path)) return true;
  }
  return false;
}

bool opdsWasSeededByChytanka() {
  FsFile file;
  if (!Storage.exists(OpdsServerStore::getFilePath()) ||
      !Storage.openFileForRead("WLC", OpdsServerStore::getFilePath(), file)) {
    return false;
  }
  // opds.json holds at most 8 servers; a bounded read is enough and avoids a
  // JsonDocument at boot. The key sits at the top level.
  char buf[1024];
  std::string text;
  const size_t size = file.size();
  text.reserve(size < 8192 ? size : 8192);
  while (text.size() < 8192) {
    const int n = file.read(buf, sizeof(buf));
    if (n <= 0) break;
    text.append(buf, static_cast<size_t>(n));
  }
  file.close();
  return text.find(OPDS_SEEDED_KEY) != std::string::npos;
}

void writeMarker(const char* reason) {
  Storage.mkdir("/.crosspoint");
  FsFile file;
  if (!Storage.openFileForWrite("WLC", MARKER_FILE, file)) {
    LOG_ERR("WLC", "Failed to write %s", MARKER_FILE);
    return;
  }
  file.write(reinterpret_cast<const uint8_t*>(reason), strlen(reason));
  file.write(reinterpret_cast<const uint8_t*>("\n"), 1);
  file.close();
  LOG_INF("WLC", "Welcome marker written (%s)", reason);
}

void drawCheckbox(const GfxRenderer& renderer, const int x, const int y, const bool checked) {
  renderer.drawRect(x, y, CHECKBOX_SIZE, CHECKBOX_SIZE, 2, true);
  if (checked) renderer.fillRect(x + 5, y + 5, CHECKBOX_SIZE - 10, CHECKBOX_SIZE - 10, true);
}

}  // namespace

bool welcomeScreenNeeded() {
  WelcomeState state;
  state.markerExists = Storage.exists(MARKER_FILE);
  if (state.markerExists) return false;
  state.settingsFileExists = anySettingsFileExists();
  state.usedChytankaBefore =
      Storage.exists(QUOTE_HISTORY_FILE) || Storage.exists(THUMB_FORMAT_MARKER_FILE) || opdsWasSeededByChytanka();
  state.languageIsUkrainian = languageIsUkrainian();
  state.readingIsRecommended = readingIsRecommended();

  switch (decideWelcome(state)) {
    case WelcomeDecision::None:
      return false;
    case WelcomeDecision::MarkOnly:
      writeMarker(!state.settingsFileExists  ? "fresh"
                  : state.usedChytankaBefore ? "upgraded-chytanka"
                                             : "already-recommended");
      return false;
    case WelcomeDecision::Show:
      LOG_INF("WLC", "Showing welcome (language uk=%d, reading recommended=%d)", state.languageIsUkrainian,
              state.readingIsRecommended);
      return true;
  }
  return false;
}

WelcomeActivity::WelcomeActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::string resumeBookPath)
    : Activity("ChytankaWelcome", renderer, mappedInput), resumeBookPath(std::move(resumeBookPath)) {}

void WelcomeActivity::onEnter() {
  Activity::onEnter();
  offerLanguage = !languageIsUkrainian();
  offerReading = !readingIsRecommended();
  keepSdFont = usesSdFont();
  // With an SD font the reader has customised reading already: offer, but
  // do not pre-select.
  applyReading = !keepSdFont;
  itemCount = 0;
  if (offerLanguage) items[itemCount++] = ITEM_LANGUAGE;
  if (offerReading) items[itemCount++] = ITEM_READING;
  items[itemCount++] = ITEM_APPLY;
  items[itemCount++] = ITEM_LATER;
  focus = itemCount - 1;  // «Пізніше»: changing nothing is the safe default
  inputArmed = false;
  requestUpdate();
}

void WelcomeActivity::loop() {
  using Button = MappedInputManager::Button;
  if (!inputArmed) {
    for (const Button b : {Button::Back, Button::Confirm, Button::Left, Button::Right, Button::Up, Button::Down}) {
      if (mappedInput.isPressed(b)) return;
    }
    inputArmed = true;
    return;
  }

  if (mappedInput.wasPressed(Button::Back)) {
    mappedInput.suppressNextBackRelease();
    finishWith(false);
    return;
  }
  if (mappedInput.wasPressed(Button::Confirm)) {
    activate();
    return;
  }
  buttonNavigator.onNextRelease([this] {
    focus = static_cast<uint8_t>(ButtonNavigator::nextIndex(focus, itemCount));
    requestUpdate();
  });
  buttonNavigator.onPreviousRelease([this] {
    focus = static_cast<uint8_t>(ButtonNavigator::previousIndex(focus, itemCount));
    requestUpdate();
  });
}

void WelcomeActivity::activate() {
  switch (items[focus]) {
    case ITEM_LANGUAGE:
      applyLanguage = !applyLanguage;
      requestUpdate();
      break;
    case ITEM_READING:
      applyReading = !applyReading;
      requestUpdate();
      break;
    case ITEM_APPLY:
      finishWith(true);
      break;
    case ITEM_LATER:
      finishWith(false);
      break;
  }
}

void WelcomeActivity::finishWith(const bool apply) {
  const bool setLanguage = apply && offerLanguage && applyLanguage;
  const bool setReading = apply && offerReading && applyReading;
  if (setLanguage) {
    RenderLock lock(*this);  // the render task reads I18N
    I18N.setLanguage(Language::UK);
    SETTINGS.language = static_cast<uint8_t>(Language::UK);
  }
  if (setReading) {
    // Global reader defaults only. Books with their own reader settings
    // (a per-book file written when the user changed settings inside that
    // book) keep them: that was an explicit choice for that book, and
    // rewriting every cached book's binary settings at boot would cost SD
    // time for little gain. New books and books without their own settings
    // follow the new defaults.
    // An SD-card font is kept: only a built-in font is switched to Bitter.
    if (!keepSdFont) SETTINGS.fontFamily = CrossPointSettings::BITTER;
    SETTINGS.hyphenationEnabled = 1;
    SETTINGS.textAntiAliasing = 0;
  }
  if (setLanguage || setReading) SETTINGS.saveToFile();
  LOG_INF("WLC", "Welcome answered: %s (language %d, reading %d)", apply ? "apply" : "later", setLanguage, setReading);
  writeMarker(apply ? "applied" : "later");
  if (resumeBookPath.empty()) {
    finish();  // root activity: ActivityManager goes Home
    return;
  }
  // Continue where the boot would have gone without the welcome: reopen the
  // book the device slept in, with main.cpp's boot-loop guard.
  APP_STATE.openEpubPath = "";
  APP_STATE.readerActivityLoadCount++;
  APP_STATE.saveToFile();
  activityManager.goToReader(resumeBookPath);
}

void WelcomeActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int side = metrics.contentSidePadding;
  const int contentWidth = pageWidth - 2 * side;
  const int h10 = renderer.getLineHeight(UI_10_FONT_ID);
  const int hSmall = renderer.getLineHeight(SMALL_FONT_ID);

  renderer.clearScreen();
  int y = metrics.topPadding + 36;

  renderer.drawCenteredText(UI_12_FONT_ID, y, TITLE_UK, true, EpdFontFamily::BOLD);
  y += renderer.getLineHeight(UI_12_FONT_ID) + 4;
  renderer.drawCenteredText(UI_10_FONT_ID, y, TITLE_EN);
  y += h10 + SECTION_GAP;

  for (const auto& line : renderer.wrappedText(UI_10_FONT_ID, INTRO_UK, contentWidth, 3)) {
    renderer.drawText(UI_10_FONT_ID, side, y, line.c_str());
    y += h10;
  }
  y += 4;
  for (const auto& line : renderer.wrappedText(SMALL_FONT_ID, INTRO_EN, contentWidth, 3)) {
    renderer.drawText(SMALL_FONT_ID, side, y, line.c_str());
    y += hSmall;
  }
  y += SECTION_GAP;

  // Options: tick box, Ukrainian label, English label (and detail).
  for (uint8_t i = 0; i < itemCount; i++) {
    const Item item = items[i];
    if (item != ITEM_LANGUAGE && item != ITEM_READING) continue;
    const bool reading = item == ITEM_READING;
    const int textX = side + OPTION_PADDING + CHECKBOX_SIZE + OPTION_PADDING;
    const int textWidth = pageWidth - side - OPTION_PADDING - textX;
    const int rowHeight = OPTION_PADDING * 2 + h10 + hSmall + (reading ? hSmall : 0);
    if (focus == i) renderer.drawRect(side, y, contentWidth, rowHeight, 2, true);
    drawCheckbox(renderer, side + OPTION_PADDING, y + OPTION_PADDING + (h10 - CHECKBOX_SIZE) / 2,
                 reading ? applyReading : applyLanguage);
    int ty = y + OPTION_PADDING;
    const char* readingUk = keepSdFont ? READING_SD_UK : READING_UK;
    const std::string uk =
        renderer.truncatedText(UI_10_FONT_ID, reading ? readingUk : LANGUAGE_UK, textWidth, EpdFontFamily::BOLD);
    renderer.drawText(UI_10_FONT_ID, textX, ty, uk.c_str(), true, EpdFontFamily::BOLD);
    ty += h10;
    if (reading) {
      const std::string detail =
          renderer.truncatedText(SMALL_FONT_ID, keepSdFont ? READING_SD_DETAIL_UK : READING_DETAIL_UK, textWidth);
      renderer.drawText(SMALL_FONT_ID, textX, ty, detail.c_str());
      ty += hSmall;
    }
    const char* readingEn = keepSdFont ? READING_SD_EN : READING_EN;
    const std::string en = renderer.truncatedText(SMALL_FONT_ID, reading ? readingEn : LANGUAGE_EN, textWidth);
    renderer.drawText(SMALL_FONT_ID, textX, ty, en.c_str());
    y += rowHeight + 10;
  }
  y += SECTION_GAP - 10;

  // «Застосувати» / «Пізніше»: the focused one is drawn filled.
  const int gap = 16;
  const int buttonWidth = (contentWidth - gap) / 2;
  for (uint8_t i = 0; i < itemCount; i++) {
    const Item item = items[i];
    if (item != ITEM_APPLY && item != ITEM_LATER) continue;
    const bool apply = item == ITEM_APPLY;
    const int bx = apply ? side : side + buttonWidth + gap;
    const bool focused = focus == i;
    if (focused) {
      renderer.fillRect(bx, y, buttonWidth, BUTTON_HEIGHT, true);
    } else {
      renderer.drawRect(bx, y, buttonWidth, BUTTON_HEIGHT, 2, true);
    }
    const char* uk = apply ? APPLY_UK : LATER_UK;
    const char* en = apply ? APPLY_EN : LATER_EN;
    const int textTop = y + (BUTTON_HEIGHT - h10 - hSmall) / 2;
    const int ukWidth = renderer.getTextWidth(UI_10_FONT_ID, uk, EpdFontFamily::BOLD);
    renderer.drawText(UI_10_FONT_ID, bx + (buttonWidth - ukWidth) / 2, textTop, uk, !focused, EpdFontFamily::BOLD);
    const int enWidth = renderer.getTextWidth(SMALL_FONT_ID, en);
    renderer.drawText(SMALL_FONT_ID, bx + (buttonWidth - enWidth) / 2, textTop + h10, en, !focused);
  }
  y += BUTTON_HEIGHT + SECTION_GAP;

  for (const auto& line : renderer.wrappedText(SMALL_FONT_ID, FOOTNOTE_UK, contentWidth, 2)) {
    renderer.drawText(SMALL_FONT_ID, side, y, line.c_str());
    y += hSmall;
  }
  for (const auto& line : renderer.wrappedText(SMALL_FONT_ID, FOOTNOTE_EN, contentWidth, 2)) {
    renderer.drawText(SMALL_FONT_ID, side, y, line.c_str());
    y += hSmall;
  }

  // Hints follow the current UI language, like every other screen.
  const bool uk = I18N.getLanguage() == Language::UK;
  const auto labels = mappedInput.mapLabels(mappedInput.withBackArrow(uk ? LATER_UK : LATER_EN), tr(STR_SELECT),
                                            tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}

}  // namespace chytanka

#endif  // CHYTANKA
