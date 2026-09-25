#pragma once

// Fork-only («Читанка» / Chytanka): one-time welcome after Chytanka is
// installed over an existing CrossInk/CrossPoint setup. Offers the Ukrainian
// UI language and the recommended reading settings (Bitter, hyphenation on,
// text anti-aliasing off); «Застосувати» applies the ticked ones, «Пізніше»
// changes nothing. Either choice writes a marker so it never shows again.
// Only compiled in when the build defines CHYTANKA.

#include <cstdint>
#include <memory>
#include <string>

#include "activities/Activity.h"
#include "util/ButtonNavigator.h"

namespace chytanka {

// Boot hook (main.cpp, routing): true when the welcome screen should replace
// Home for this boot. Writes the marker itself when the screen is not needed
// (fresh SD card, earlier Chytanka release, nothing to offer).
bool welcomeScreenNeeded();

// Boot hook (main.cpp), called before this boot writes any files of its own:
// records which traces of an earlier Chytanka release were already on the SD
// card (this build writes the same thumbnail marker at every boot).
void captureEarlierInstallTraces();

class WelcomeActivity final : public Activity {
 public:
  // resumeBookPath: the book this boot would have reopened (the sleep-from-
  // reader resume path in main.cpp), or empty to continue Home once answered.
  WelcomeActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::string resumeBookPath);

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  enum Item : uint8_t { ITEM_LANGUAGE, ITEM_READING, ITEM_APPLY, ITEM_LATER };

  ButtonNavigator buttonNavigator;
  Item items[4] = {};
  uint8_t itemCount = 0;
  uint8_t focus = 0;
  bool offerLanguage = false;
  bool offerReading = false;
  // The reader uses an SD-card font: the reading option then leaves the font
  // alone and only offers hyphenation and anti-aliasing off, unticked.
  bool keepSdFont = false;
  bool applyLanguage = true;
  bool applyReading = true;
  std::string resumeBookPath;
  // Input is ignored until every button has been seen released once, so a
  // button still held from boot cannot pick an option by itself.
  bool inputArmed = false;

  void activate();
  void finishWith(bool apply);
};

}  // namespace chytanka
